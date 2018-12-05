// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binary_encoding.h"

#include <array>
#include <string>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAreArray;

namespace inspector_protocol {
TEST(EncodeDecodeUTF16StringTest, RoundtripsEmpty) {
  // This roundtrips the empty utf16 string through the pair of EncodeUTF16 /
  // EncodeUTF16 functions.
  std::vector<uint8_t> encoded;
  EncodeUTF16String(span<uint16_t>(), &encoded);
  EXPECT_EQ(size_t(1), encoded.size());
  // first three bits: major type = 2; remaining five bits: additional info =
  // size 0.
  EXPECT_EQ(2 << 5, encoded[0]);

  // Now the reverse direction: decode the encoded empty string and store it
  // into decoded.
  std::vector<uint16_t> decoded;
  span<uint8_t> encoded_bytes = span<uint8_t>(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUTF16String(&encoded_bytes, &decoded));
  EXPECT_TRUE(decoded.empty());
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUTF16StringTest, RoundtripsHelloWorld) {
  // This roundtrips the hello world message which is given here in utf16
  // characters. 0xd83c, 0xdf0e: UTF16 encoding for the "Earth Globe Americas"
  // character, 🌎.
  std::array<uint16_t, 10> msg{
      {'H', 'e', 'l', 'l', 'o', ',', ' ', 0xd83c, 0xdf0e, '.'}};
  std::vector<uint8_t> encoded;
  EncodeUTF16String(span<uint16_t>(msg.data(), msg.size()), &encoded);
  // This will be encoded as BYTE_STRING of length 20, so the 20 is encoded in
  // the additional info part of the initial byte. Payload is two bytes for each
  // UTF16 character.
  uint8_t initial_byte = /*major type=*/2 << 5 | /*additional info=*/20;
  std::array<uint8_t, 21> encoded_expected = {
      {initial_byte, 'H', 0,   'e', 0,    'l',  0,    'l',  0,   'o', 0,
       ',',          0,   ' ', 0,   0x3c, 0xd8, 0x0e, 0xdf, '.', 0}};
  EXPECT_THAT(encoded, ElementsAreArray(encoded_expected));

  // Now decode to complete the roundtrip.
  std::vector<uint16_t> decoded;
  span<uint8_t> encoded_bytes = span<uint8_t>(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUTF16String(&encoded_bytes, &decoded));
  EXPECT_THAT(decoded, ElementsAreArray(msg));
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUTF16StringTest, Roundtrips500) {
  // We roundtrip a message that has 250 16 bit values. Each of these are just
  // set to their index. 250 is interesting because the cbor spec uses a
  // BYTE_STRING of length 500 for one of their examples of how to encode the
  // start of it (section 2.1) so it's easy for us to look at the first three
  // bytes closely.
  std::vector<uint16_t> two_fifty;
  for (uint16_t ii = 0; ii < 250; ++ii) two_fifty.push_back(ii);
  std::vector<uint8_t> encoded;
  EncodeUTF16String(span<uint16_t>(two_fifty.data(), two_fifty.size()),
                    &encoded);
  EXPECT_EQ(size_t(3 + 250 * 2), encoded.size());
  // Now check the first three bytes:
  // Major type: 2 (BYTE_STRING)
  // Additional information: 25, indicating size is represented by 2 bytes.
  // Bytes 1 and 2 encode 500 (0x01f4).
  EXPECT_EQ(2 << 5 | 25, encoded[0]);
  EXPECT_EQ(0x01, encoded[1]);
  EXPECT_EQ(0xf4, encoded[2]);

  // Now decode to complete the roundtrip.
  std::vector<uint16_t> decoded;
  span<uint8_t> encoded_bytes = span<uint8_t>(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUTF16String(&encoded_bytes, &decoded));
  EXPECT_THAT(decoded, ElementsAreArray(two_fifty));
  EXPECT_TRUE(encoded_bytes.empty());
}
}  // namespace inspector_protocol
