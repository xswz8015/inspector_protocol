// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_CBOR_H_
#define INSPECTOR_PROTOCOL_ENCODING_CBOR_H_

#include <cstdint>
#include <memory>
#include <vector>
#include "json_parser_handler.h"
#include "span.h"
#include "status.h"

namespace inspector_protocol {
// The binary encoding for the inspector protocol follows the CBOR specification
// (RFC 7049). Additional constraints:
// - Only indefinite length maps and arrays are supported.
// - At the top level, a message must be an indefinite length map.
// - For scalars, we support only the int32_t range, encoded as
//   UNSIGNED/NEGATIVE (major types 0 / 1).
// - UTF16 strings, including with unbalanced surrogate pairs, are encoded
//   as CBOR BYTE_STRING (major type 2). For such strings, the number of
//   bytes encoded must be even.
// - UTF8 strings (major type 3) may only have ASCII characters
//   (7 bit US-ASCII).
// - Arbitrary byte arrays, in the inspector protocol called 'binary',
//   are encoded as BYTE_STRING (major type 2), prefixed with a byte
//   indicating base64 when rendered as JSON.

// Encodes |value| as UNSIGNED (major type 0).
void EncodeUnsigned(uint64_t value, std::vector<uint8_t>* out);

// Decodes |value| from |bytes|, assuming that it's encoded as UNSIGNED
// (major type 0). Iff successful, updates |bytes| to position after
// the uint and returns true.
bool DecodeUnsigned(span<uint8_t>* bytes, uint64_t* value);

// Encodes |value| as |UNSIGNED| (major type 0) iff >= 0, or |NEGATIVE|
// (major type 1) iff < 0.
void EncodeSigned(int32_t value, std::vector<uint8_t>* out);

// Decodes |value| from |bytes|, if it's encoded as either UNSIGNED
// or NEGATIVE and within range of int32_t. Otherwise returns false.
bool DecodeSigned(span<uint8_t>* bytes, int32_t* value);

// Encodes a UTF16 string as a BYTE_STRING (major type 2). Each utf16
// character in |in| is emitted with most significant byte first,
// appending to |out|.
void EncodeUTF16String(span<uint16_t> in, std::vector<uint8_t>* out);

// Given an encoded BYTE_STRING (major type 2) with definitive length at the
// beginning of |bytes|, extracts the characters into |str| while interpreting
// the leading byte as the least significant one for each character. Updates
// |bytes| to the first byte after the encoded byte string. Returns true iff
// successful.
bool DecodeUTF16String(span<uint8_t>* bytes, std::vector<uint16_t>* str);

// Encodes a UTF8 string |in| as STRING (major type 3).
void EncodeUTF8String(span<uint8_t> in, std::vector<uint8_t>* out);
// Given an encoded STRING (major type 3) with definitive length at the
// beginning of |bytes|, extracts the bytes into |str|. Updates |bytes|
// to the first byte after the encoded byte string. Returns true iff successful.
bool DecodeUTF8String(span<uint8_t>* bytes, std::vector<uint8_t>* str);

// Encodes arbitrary binary data in |in| as a BYTE_STRING (major type 2) with
// definitive length, prefixed with tag 22 indicating expected conversion to
// base64 (see RFC 7049, Table 3 and Section 2.4.4.2).
void EncodeBinary(span<uint8_t> in, std::vector<uint8_t>* out);
bool DecodeBinary(span<uint8_t>* bytes, std::vector<uint8_t>* out);

// Encodes / decodes a double as Major type 7 (SIMPLE_VALUE),
// with additional info = 27, followed by 8 bytes in big endian.
void EncodeDouble(double value, std::vector<uint8_t>* out);
bool DecodeDouble(span<uint8_t>* bytes, double* value);

// This can be used to convert from JSON to CBOR, by passing the
// return value to the routines in json_parser.h.  The handler will encode into
// |out|, and iff an error occurs it will set |status| to an error and clear
// |out|. Otherwise, |status.ok()| will be |true|.
std::unique_ptr<JsonParserHandler> NewJsonToCBOREncoder(
    std::vector<uint8_t>* out, Status* status);

// Parses a CBOR encoded message from |bytes|, sending JSON events to
// |json_out|. If an error occurs, sends |out->HandleError|, and parsing stops.
// The client is responsible for discarding the already received information in
// that case.
void ParseCBOR(span<uint8_t> bytes, JsonParserHandler* json_out);
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_CBOR_H_
