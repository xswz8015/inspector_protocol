// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_BINARY_ENCODING_H_
#define INSPECTOR_PROTOCOL_ENCODING_BINARY_ENCODING_H_

#include <cstdint>
#include <memory>
#include <vector>
#include "json_parser_handler.h"
#include "span.h"

namespace inspector_protocol {
// The binary encoding for the inspector protocol follows the CBOR specification
// (RFC 7049).

// Encodes |value| as UNSIGNED (major type 0).
void EncodeUnsigned(uint64_t value, std::vector<uint8_t>* out);

// Decodes |value| from |bytes|, assuming that it's encoded as UNSIGNED
// (major type 0). Iff successful, updates |bytes| to position after
// the uint and returns true.
bool DecodeUnsigned(span<uint8_t>* bytes, uint64_t* value);

// Encodes |value| as |UNSIGNED| (major type 0) iff >= 0, or |NEGATIVE|
// (major type 1) iff < 0.
void EncodeSigned(int32_t value, std::vector<uint8_t>* out);

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

// Encodes / decodes a double as Major type 7 (SIMPLE_VALUE),
// with additional info = 27, followed by 8 bytes in big endian.
void EncodeDouble(double value, std::vector<uint8_t>* out);
bool DecodeDouble(span<uint8_t>* bytes, double* value);

// This can be used to convert from JSON to this binary encoding,
// by passing the return value to the routines in json_parser.h.
// The handler will encode into |out|, and iff an error occurs it
// will set |error| to true and clear |out|. Otherwise, |error| will
// be set to |false|.
std::unique_ptr<JsonParserHandler> NewJsonToBinaryEncoder(
    std::vector<uint8_t>* out, bool* error);
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_BINARY_ENCODING_H_
