// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binary_encoding.h"

#include <cassert>
#include <limits>
#include "json_parser_handler.h"

namespace inspector_protocol {
namespace {
// The major types from RFC 7049 Section 2.1.
enum class MajorType {
  UNSIGNED = 0,
  NEGATIVE = 1,
  BYTE_STRING = 2,
  STRING = 3,
  ARRAY = 4,
  MAP = 5,
  TAG = 6,
  SIMPLE_VALUE = 7
};

// Indicates the number of bits the "initial byte" needs to be shifted to the
// right after applying |kMajorTypeMask| to produce the major type in the
// lowermost bits.
static constexpr uint8_t kMajorTypeBitShift = 5u;
// Mask selecting the low-order 5 bits of the "initial byte", which is where
// the additional information is encoded.
static constexpr uint8_t kAdditionalInformationMask = 0x1f;
// Mask selecting the high-order 3 bits of the "initial byte", which indicates
// the major type of the encoded value.
static constexpr uint8_t kMajorTypeMask = 0xe0;
// Indicates the integer is in the following byte.
static constexpr uint8_t kAdditionalInformation1Byte = 24u;
// Indicates the integer is in the next 2 bytes.
static constexpr uint8_t kAdditionalInformation2Bytes = 25u;
// Indicates the integer is in the next 4 bytes.
static constexpr uint8_t kAdditionalInformation4Bytes = 26u;
// Indicates the integer is in the next 8 bytes.
static constexpr uint8_t kAdditionalInformation8Bytes = 27u;

// Encodes the initial byte, consisting of the |type| in the first 3 bits
// followed by 5 bits of |additional_info|.
constexpr uint8_t EncodeInitialByte(MajorType type, uint8_t additional_info) {
  return (uint8_t(type) << kMajorTypeBitShift) |
         (additional_info & kAdditionalInformationMask);
}

// See RFC 7049 Section 2.3, Table 2.
static constexpr uint8_t kEncodedTrue =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 21);
static constexpr uint8_t kEncodedFalse =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 20);
static constexpr uint8_t kEncodedNull =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 22);
static constexpr uint8_t kInitialByteForDouble =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 27);

// See RFC 7049 Section 2.2.1, indefinite length arrays / maps have additional
// info = 31.
static constexpr uint8_t kInitialByteIndefiniteLengthArray =
    EncodeInitialByte(MajorType::ARRAY, 31);
static constexpr uint8_t kInitialByteIndefiniteLengthMap =
    EncodeInitialByte(MajorType::MAP, 31);
// See RFC 7049 Section 2.3, Table 1; this is used for finishing indefinite
// length maps / arrays.
static constexpr uint8_t kStopByte =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 31);

// When parsing binary (CBOR), we limit recursion depth for objects and arrays
// to this constant.
static constexpr int kStackLimit = 1000;

// Writes the bytes for |v| to |out|, starting with the most significant byte.
// See also: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
template <typename T>
void WriteBytesMostSignificantByteFirst(T v, std::vector<uint8_t>* out) {
  for (int shift_bytes = sizeof(T) - 1; shift_bytes >= 0; --shift_bytes)
    out->push_back(0xff & v >> (shift_bytes * 8));
}

// Writes the start of an item with |type|. The |value| may indicate the size,
// or it may be the payload if the value is an unsigned integer.
void WriteItemStart(MajorType type, uint64_t value,
                    std::vector<uint8_t>* encoded) {
  if (value < 24) {
    // Values 0-23 are encoded directly into the additional info of the
    // initial byte.
    encoded->push_back(EncodeInitialByte(type, /*additiona_info=*/value));
    return;
  }
  if (value <= std::numeric_limits<uint8_t>::max()) {
    // Values 24-255 are encoded with one initial byte, followed by the value.
    encoded->reserve(encoded->size() + 1 + sizeof(uint8_t));
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation1Byte));
    encoded->push_back(value);
    return;
  }
  if (value <= std::numeric_limits<uint16_t>::max()) {
    // Values 256-65535: 1 initial byte + 2 bytes payload.
    encoded->reserve(encoded->size() + 1 + sizeof(uint16_t));
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation2Bytes));
    WriteBytesMostSignificantByteFirst<uint16_t>(value, encoded);
    return;
  }
  if (value <= std::numeric_limits<uint32_t>::max()) {
    // 32 bit uint: 1 initial byte + 4 bytes payload.
    encoded->reserve(encoded->size() + 1 + sizeof(uint32_t));
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation4Bytes));
    WriteBytesMostSignificantByteFirst<uint32_t>(value, encoded);
    return;
  }
  // 64 bit uint: 1 initial byte + 8 bytes payload.
  encoded->reserve(encoded->size() + 1 + sizeof(uint64_t));
  encoded->push_back(EncodeInitialByte(type, kAdditionalInformation8Bytes));
  WriteBytesMostSignificantByteFirst<uint64_t>(value, encoded);
}

// Extracts sizeof(T) bytes from |in| to extract a value of type T
// (e.g. uint64_t, uint32_t, ...), most significant byte first.
// See also: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
template <typename T>
T ReadBytesMostSignificantByteFirst(span<uint8_t> in) {
  assert(size_t(in.size()) >= sizeof(T));
  T result = 0;
  for (size_t shift_bytes = 0; shift_bytes < sizeof(T); ++shift_bytes)
    result |= T(in[sizeof(T) - 1 - shift_bytes]) << (shift_bytes * 8);
  return result;
}

// Reads the start of an item with definitive size from |in|.
// |type| is the major type as specified in RFC 7049 Section 2.1.
// |value| is the payload (e.g. for MajorType::UNSIGNED) or is the size
// (e.g. for BYTE_STRING). Remainder is the first byte after
// the item start, that is, the first byte after the encoded value / size.
// Returns true iff successful.
bool ReadItemStart(span<uint8_t>* bytes, MajorType* type, uint64_t* value) {
  if (bytes->empty()) return false;
  uint8_t initial_byte = (*bytes)[0];
  *type = MajorType((initial_byte & kMajorTypeMask) >> kMajorTypeBitShift);

  uint8_t additional_information = initial_byte & kAdditionalInformationMask;
  if (additional_information < 24) {
    // Values 0-23 are encoded directly into the additional info of the
    // initial byte.
    *value = additional_information;
    *bytes = bytes->subspan(1);
    return true;
  }
  if (additional_information == kAdditionalInformation1Byte) {
    // Values 24-255 are encoded with one initial byte, followed by the value.
    if (bytes->size() < 2) return false;
    *value = ReadBytesMostSignificantByteFirst<uint8_t>(bytes->subspan(1));
    *bytes = bytes->subspan(2);
    return true;
  }
  if (additional_information == kAdditionalInformation2Bytes) {
    // Values 256-65535: 1 initial byte + 2 bytes payload.
    if (static_cast<size_t>(bytes->size()) < 1 + sizeof(uint16_t)) return false;
    *value = ReadBytesMostSignificantByteFirst<uint16_t>(bytes->subspan(1));
    *bytes = bytes->subspan(1 + sizeof(uint16_t));
    return true;
  }
  if (additional_information == kAdditionalInformation4Bytes) {
    // 32 bit uint: 1 initial byte + 4 bytes payload.
    if (static_cast<size_t>(bytes->size()) < 1 + sizeof(uint32_t)) return false;
    *value = ReadBytesMostSignificantByteFirst<uint32_t>(bytes->subspan(1));
    *bytes = bytes->subspan(1 + sizeof(uint32_t));
    return true;
  }
  if (additional_information == kAdditionalInformation8Bytes) {
    // 64 bit uint: 1 initial byte + 8 bytes payload.
    if (static_cast<size_t>(bytes->size()) < 1 + sizeof(uint64_t)) return false;
    *value = ReadBytesMostSignificantByteFirst<uint64_t>(bytes->subspan(1));
    *bytes = bytes->subspan(1 + sizeof(uint64_t));
    return true;
  }
  return false;
}
}  // namespace

void EncodeUnsigned(uint64_t value, std::vector<uint8_t>* out) {
  WriteItemStart(MajorType::UNSIGNED, value, out);
}

bool DecodeUnsigned(span<uint8_t>* bytes, uint64_t* value) {
  MajorType type;
  span<uint8_t> internal_bytes = *bytes;
  uint64_t internal_value;
  if (!ReadItemStart(&internal_bytes, &type, &internal_value)) return false;
  if (type != MajorType::UNSIGNED) return false;
  *bytes = internal_bytes;
  *value = internal_value;
  return true;
}

namespace internal {
// The following two routines implement encoding / decoding for NEGATIVE,
// Major Type 1 (see RFC 7049, Section 2.1). However, we will (thus far)
// only be using int32_t values anyway, so the method exposed in the
// header instead EncodeSigned (below), which dispatches between
// UNSIGNED and NEGATIVE.

// Encodes |value| as NEGATIVE (major type 1). |value| must be negative.
void EncodeNegative(int64_t value, std::vector<uint8_t>* out) {
  assert(value < 0);
  WriteItemStart(MajorType::NEGATIVE, static_cast<uint64_t>(-(value + 1)), out);
}

// Decodes |value| from |bytes|, assuming that it's encoded as NEGATIVE.
// (major type 1). Iff successful, updates |bytes| to position after
// the negative int and returns true.
bool DecodeNegative(span<uint8_t>* bytes, int64_t* value) {
  MajorType type;
  span<uint8_t> internal_bytes = *bytes;
  uint64_t encoded_value;
  if (!ReadItemStart(&internal_bytes, &type, &encoded_value)) return false;
  if (type != MajorType::NEGATIVE) return false;
  *value = -static_cast<int64_t>(encoded_value) - 1;
  *bytes = internal_bytes;
  return true;
}
}  // namespace internal

void EncodeSigned(int32_t value, std::vector<uint8_t>* out) {
  if (value >= 0)
    EncodeUnsigned(value, out);
  else
    internal::EncodeNegative(value, out);
}

bool DecodeSigned(span<uint8_t>* bytes, int32_t* value) {
  MajorType type;
  span<uint8_t> internal_bytes = *bytes;
  uint64_t encoded_value;
  if (!ReadItemStart(&internal_bytes, &type, &encoded_value)) return false;
  // It's unfortunate that we're rejecting perfectly fine CBOR encoded UNSIGNED
  // / NEGATIVE values here if they're outside the range of int32_t. This is
  // (for now) for compatibility with JSON, or more specifically with what our
  // parser supports via JsonParserHandler::HandleInt.
  if (type == MajorType::UNSIGNED) {
    if (encoded_value <= std::numeric_limits<int32_t>::max()) {
      *value = encoded_value;
      *bytes = internal_bytes;
      return true;
    }
  } else if (type == MajorType::NEGATIVE) {
    int64_t decoded_value = -static_cast<int64_t>(encoded_value) - 1;
    if (decoded_value >= std::numeric_limits<int32_t>::min()) {
      *value = decoded_value;
      *bytes = internal_bytes;
      return true;
    }
  }
  return false;
}

void EncodeUTF16String(span<uint16_t> in, std::vector<uint8_t>* out) {
  WriteItemStart(MajorType::BYTE_STRING, static_cast<uint64_t>(in.size_bytes()),
                 out);
  // When emitting UTF16 characters, we always write the least significant byte
  // first; this is because it's the native representation for X86.
  // TODO(johannes): Implement a more efficient thing here later, e.g.
  // casting *iff* the machine has this byte order.
  // The wire format for UTF16 chars will probably remain the same
  // (least significant byte first) since this way we can have
  // golden files, unittests, etc. that port easily and universally.
  // See also:
  // https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
  for (const uint16_t two_bytes : in) {
    out->push_back(two_bytes);
    out->push_back(two_bytes >> 8);
  }
}

bool DecodeUTF16String(span<uint8_t>* bytes, std::vector<uint16_t>* str) {
  MajorType type;
  uint64_t num_bytes;
  span<uint8_t> internal_bytes = *bytes;  // only written upon success
  if (!ReadItemStart(&internal_bytes, &type, &num_bytes)) return false;
  if (type != MajorType::BYTE_STRING) return false;
  if (static_cast<size_t>(internal_bytes.size()) < num_bytes) return false;
  // Must be divisible by 2 since UTF16 is 2 bytes per character.
  if (num_bytes & 1) return false;
  assert(str->empty());
  str->reserve(num_bytes);
  for (size_t ii = 0; ii < num_bytes; ii += 2) {
    uint16_t high_byte = internal_bytes[ii + 1];
    uint16_t low_byte = internal_bytes[ii];
    str->push_back((high_byte << 8) | low_byte);
  }
  *bytes = internal_bytes.subspan(num_bytes);
  return true;
}

// A double is encoded with a specific initial byte
// (kInitialByteForDouble) plus the 64 bits of payload for its value.
constexpr int kEncodedDoubleSize = 1 + sizeof(uint64_t);

void EncodeDouble(double value, std::vector<uint8_t>* out) {
  // The additional_info=27 indicates 64 bits for the double follow.
  // See RFC 7049 Section 2.3, Table 1.
  out->reserve(out->size() + kEncodedDoubleSize);
  out->push_back(kInitialByteForDouble);
  union {
    double from_double;
    uint64_t to_uint64;
  } reinterpret;
  reinterpret.from_double = value;
  WriteBytesMostSignificantByteFirst<uint64_t>(reinterpret.to_uint64, out);
}

bool DecodeDouble(span<uint8_t>* bytes, double* value) {
  if (bytes->size() < kEncodedDoubleSize) return false;
  if ((*bytes)[0] != kInitialByteForDouble) return false;
  union {
    uint64_t from_uint64;
    double to_double;
  } reinterpret;
  reinterpret.from_uint64 =
      ReadBytesMostSignificantByteFirst<uint64_t>(bytes->subspan(1));
  *value = reinterpret.to_double;
  *bytes = bytes->subspan(kEncodedDoubleSize);
  return true;
}

namespace {
class JsonToBinaryEncoder : public JsonParserHandler {
 public:
  JsonToBinaryEncoder(std::vector<uint8_t>* out, Status* status)
      : out_(out), status_(status) {
    *status_ = Status();
  }

  void HandleObjectBegin() override {
    out_->push_back(kInitialByteIndefiniteLengthMap);
  }

  void HandleObjectEnd() override { out_->push_back(kStopByte); };

  void HandleArrayBegin() override {
    out_->push_back(kInitialByteIndefiniteLengthArray);
  }

  void HandleArrayEnd() override { out_->push_back(kStopByte); };

  void HandleString(std::vector<uint16_t> chars) override {
    EncodeUTF16String(span<uint16_t>(chars.data(), chars.size()), out_);
  }

  void HandleDouble(double value) override { EncodeDouble(value, out_); };

  void HandleInt(int32_t value) override { EncodeSigned(value, out_); }

  void HandleBool(bool value) override {
    // See RFC 7049 Section 2.3, Table 2.
    out_->push_back(value ? kEncodedTrue : kEncodedFalse);
  }

  void HandleNull() override {
    // See RFC 7049 Section 2.3, Table 2.
    out_->push_back(kEncodedNull);
  }

  void HandleError(Status error) override {
    assert(!error.ok());
    *status_ = error;
    out_->clear();
  }

 private:
  std::vector<uint8_t>* out_;
  Status* status_;
};
}  // namespace

std::unique_ptr<JsonParserHandler> NewJsonToBinaryEncoder(
    std::vector<uint8_t>* out, Status* status) {
  return std::make_unique<JsonToBinaryEncoder>(out, status);
}

namespace {
// Below are three parsing routines for CBOR / binary, which cover enough
// to roundtrip JSON messages.
Error ParseMap(int32_t stack_depth, span<uint8_t>* bytes,
               JsonParserHandler* out);
Error ParseArray(int32_t stack_depth, span<uint8_t>* bytes,
                 JsonParserHandler* out);
Error ParseValue(int32_t stack_depth, span<uint8_t>* bytes,
                 JsonParserHandler* out);

Error ParseValue(int32_t stack_depth, span<uint8_t>* bytes,
                 JsonParserHandler* out) {
  if (stack_depth > kStackLimit)
    return Error::BINARY_ENCODING_STACK_LIMIT_EXCEEDED;
  if (bytes->empty())
    return Error::BINARY_ENCODING_UNEXPECTED_EOF_EXPECTED_VALUE;
  // First we dispatch on the entire initial byte. Only when this doesn't
  // give satisfaction do we use the major types (first three bits)
  // to dispatch between a few more choices below.
  switch ((*bytes)[0]) {
    case kEncodedTrue:
      out->HandleBool(true);
      *bytes = bytes->subspan(1);
      return Error::OK;
    case kEncodedFalse:
      out->HandleBool(false);
      *bytes = bytes->subspan(1);
      return Error::OK;
    case kEncodedNull:
      out->HandleNull();
      *bytes = bytes->subspan(1);
      return Error::OK;
    case kInitialByteForDouble: {
      double value;
      if (!DecodeDouble(bytes, &value))
        return Error::BINARY_ENCODING_INVALID_DOUBLE;
      out->HandleDouble(value);
      return Error::OK;
    }
    case kInitialByteIndefiniteLengthArray:
      return ParseArray(stack_depth + 1, bytes, out);
    case kInitialByteIndefiniteLengthMap:
      return ParseMap(stack_depth + 1, bytes, out);
    default:
      break;
  }
  switch ((*bytes)[0] >> kMajorTypeBitShift) {
    case uint8_t(MajorType::UNSIGNED):
    case uint8_t(MajorType::NEGATIVE): {
      int32_t value;
      if (!DecodeSigned(bytes, &value))
        return Error::BINARY_ENCODING_INVALID_SIGNED;
      out->HandleInt(value);
      return Error::OK;
    }
    case uint8_t(MajorType::BYTE_STRING): {
      std::vector<uint16_t> value;
      if (!DecodeUTF16String(bytes, &value))
        return Error::BINARY_ENCODING_INVALID_STRING16;
      out->HandleString(std::move(value));
      return Error::OK;
    }
    case uint8_t(MajorType::STRING):        // utf8, todo
    case uint8_t(MajorType::ARRAY):         // indef length case handled above
    case uint8_t(MajorType::MAP):           // indef length case handled above
    case uint8_t(MajorType::TAG):           // todo
    case uint8_t(MajorType::SIMPLE_VALUE):  // supported cases handled above
    default:
      return Error::BINARY_ENCODING_UNSUPPORTED_VALUE;
  }
}

// |bytes| must start with the indefinite length array byte, so basically,
// ParseArray may only be called after an indefinite length array has been
// detected.
Error ParseArray(int32_t stack_depth, span<uint8_t>* bytes,
                 JsonParserHandler* out) {
  assert(!bytes->empty());
  assert((*bytes)[0] == kInitialByteIndefiniteLengthArray);

  *bytes = bytes->subspan(1);
  out->HandleArrayBegin();
  while (!bytes->empty()) {
    // Parse end of array.
    if ((*bytes)[0] == kStopByte) {
      out->HandleArrayEnd();
      return Error::OK;
    }
    // Parse value.
    Error status = ParseValue(stack_depth, bytes, out);
    if (status != Error::OK) return status;
  }
  return Error::BINARY_ENCODING_UNEXPECTED_EOF_IN_ARRAY;
}

// |bytes| must start with the indefinite length array byte, so basically,
// ParseArray may only be called after an indefinite length array has been
// detected.
Error ParseMap(int32_t stack_depth, span<uint8_t>* bytes,
               JsonParserHandler* out) {
  assert(!bytes->empty());
  assert((*bytes)[0] == kInitialByteIndefiniteLengthMap);

  *bytes = bytes->subspan(1);
  out->HandleObjectBegin();
  while (!bytes->empty()) {
    // Parse end of map.
    if ((*bytes)[0] == kStopByte) {
      out->HandleObjectEnd();
      return Error::OK;
    }
    // Parse key.
    std::vector<uint16_t> key;
    if (!DecodeUTF16String(bytes, &key))
      return Error::BINARY_ENCODING_INVALID_MAP_KEY;
    out->HandleString(std::move(key));
    // Parse value.
    Error status = ParseValue(stack_depth, bytes, out);
    if (status != Error::OK) return status;
  }
  return Error::BINARY_ENCODING_UNEXPECTED_EOF_IN_MAP;
}
}  // namespace

void ParseBinary(span<uint8_t> bytes, JsonParserHandler* json_out) {
  if (bytes.empty()) {
    json_out->HandleError(Status{Error::BINARY_ENCODING_NO_INPUT, 0});
    return;
  }
  if (bytes[0] != kInitialByteIndefiniteLengthMap) {
    json_out->HandleError(Status{Error::BINARY_ENCODING_INVALID_START_BYTE, 0});
    return;
  }
  span<uint8_t> internal_bytes = bytes;
  Error error = ParseMap(/*stack_depth=*/1, &internal_bytes, json_out);
  if (error == Error::OK) return;
  json_out->HandleError(Status{error, bytes.size() - internal_bytes.size()});
}
}  // namespace inspector_protocol
