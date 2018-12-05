// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binary_encoding.h"

#include <cassert>
#include <limits>

namespace inspector_protocol {
namespace {
// The major types from section 2.1.
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
uint8_t EncodeInitialByte(MajorType type, uint8_t additional_info) {
  return (static_cast<uint8_t>(type) << kMajorTypeBitShift) |
         (additional_info & kAdditionalInformationMask);
}

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
  assert(in.size() >= sizeof(T));
  T result = 0;
  for (size_t shift_bytes = 0; shift_bytes < sizeof(T); ++shift_bytes)
    result |= T(in[sizeof(T) - 1 - shift_bytes]) << (shift_bytes * 8);
  return result;
}

// Reads the start of an item with definitive size from |in|.
// |type| is the major type as specified in section 2.1.
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
}  // namespace inspector_protocol
