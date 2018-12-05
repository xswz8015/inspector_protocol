// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_BINARY_ENCODING_H_
#define INSPECTOR_PROTOCOL_ENCODING_BINARY_ENCODING_H_

#include <cstdint>
#include <vector>
#include "span.h"

namespace inspector_protocol {
// The binary encoding for the inspector protocol follows the CBOR specification
// (RFC7049).

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
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_BINARY_ENCODING_H_
