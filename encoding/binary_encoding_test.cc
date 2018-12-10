// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binary_encoding.h"

#include <array>
#include <string>
#include "base/strings/stringprintf.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json_parser.h"
#include "linux_dev_platform.h"

using testing::ElementsAreArray;

namespace inspector_protocol {
TEST(EncodeDecodeUnsignedTest, Roundtrips23) {
  // This roundtrips the uint64_t value 23 through the pair of EncodeUnsigned /
  // DecodeUnsigned functions; this is interesting since 23 is encoded as
  // a single byte.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(23, &encoded);
  // first three bits: major type = 0; remaining five bits: additional info =
  // value 23.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 1>{{23}}));

  // Now the reverse direction: decode the encoded empty string and store it
  // into |decoded|.
  uint64_t decoded = 0;  // Assign != 23 to ensure it's not 23 by accident.
  span<uint8_t> encoded_bytes(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(23), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint8) {
  // This roundtrips the uint64_t value 42 through the pair of EncodeUnsigned /
  // EncodeUnsigned functions. This is different from Roundtrip0 because
  // 42 is encoded in an extra byte after the initial one.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(42, &encoded);
  // first three bits: major type = 0;
  // remaining five bits: additional info = 24, indicating payload is uint8.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 2>{{24, 42}}));

  // Reverse direction.
  uint64_t decoded = 0;
  span<uint8_t> encoded_bytes(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(42), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint16) {
  // 500 is encoded as a uint16 after the initial byte.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(500, &encoded);
  EXPECT_EQ(size_t(3), encoded.size());  // 1 for initial byte, 2 for uint16.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 25, indicating payload is uint16.
  EXPECT_EQ(25, encoded[0]);
  EXPECT_EQ(0x01, encoded[1]);
  EXPECT_EQ(0xf4, encoded[2]);

  // Reverse direction.
  uint64_t decoded;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(500), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint32) {
  // 0xdeadbeef is encoded as a uint32 after the initial byte.
  std::vector<uint8_t> encoded;
  EncodeUnsigned(0xdeadbeef, &encoded);
  // 1 for initial byte, 4 for the uint32.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 26, indicating payload is uint32.
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 5>{{26, 0xde, 0xad, 0xbe, 0xef}}));

  // Reverse direction.
  uint64_t decoded;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(uint64_t(0xdeadbeef), decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, RoundtripsUint64) {
  // 0xaabbccddeeff0011 is encoded as a uint64 after the initial byte
  std::vector<uint8_t> encoded;
  EncodeUnsigned(0xaabbccddeeff0011, &encoded);
  // 1 for initial byte, 8 for the uint64.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 27, indicating payload is uint64.
  EXPECT_THAT(encoded,
              ElementsAreArray(std::array<uint8_t, 9>{
                  {27, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11}}));

  // Reverse direction.
  uint64_t decoded;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeUnsigned(&encoded_bytes, &decoded));
  EXPECT_EQ(0xaabbccddeeff0011, decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeUnsignedTest, ErrorCases) {
  struct TestCase {
    std::vector<uint8_t> data;
    std::string msg;
  };
  std::vector<TestCase> tests{
      {TestCase{
           {24},
           "additional info = 24 would require 1 byte of payload (but it's 0)"},
       TestCase{{27, 0xaa, 0xbb, 0xcc},
                "additional info = 27 would require 8 bytes of payload (but "
                "it's 3)"},
       TestCase{{2 << 5}, "we require major type 0 (but it's 2)"},
       TestCase{{29}, "additional info = 29 isn't recognized"}}};
  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.msg);
    uint64_t decoded = 0xdeadbeef;  // unlikely to be written by accident.
    span<uint8_t> encoded_bytes(&test.data[0], test.data.size());
    EXPECT_FALSE(DecodeUnsigned(&encoded_bytes, &decoded));
    EXPECT_EQ(0xdeadbeef, decoded);  // unmodified
    EXPECT_EQ(test.data.size(), size_t(encoded_bytes.size()));
  }
}

namespace internal {
// EncodeNegative / DecodeNegative are not exposed in the header, but we
// still want to test a roundtrip here.
void EncodeNegative(int64_t value, std::vector<uint8_t>* out);
bool DecodeNegative(span<uint8_t>* bytes, int64_t* value);

TEST(EncodeDecodeNegativeTest, RoundtripsMinus24) {
  // This roundtrips the int64_t value -24 through the pair of EncodeNegative /
  // DecodeNegative functions; this is interesting since -24 is encoded as
  // a single byte, and it tests the specific encoding (note how for unsigned
  // the single byte covers values up to 23).
  // Additional examples are covered in RoundtripsAdditionalExamples.
  std::vector<uint8_t> encoded;
  EncodeNegative(-24, &encoded);
  // first three bits: major type = 0; remaining five bits: additional info =
  // value 23.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 1>{{1 << 5 | 23}}));

  // Now the reverse direction: decode the encoded empty string and store it
  // into decoded.
  int64_t decoded = 0;  // Assign != 23 to ensure it's not 23 by accident.
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeNegative(&encoded_bytes, &decoded));
  EXPECT_EQ(-24, decoded);
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeNegativeTest, RoundtripsAdditionalExamples) {
  std::vector<int64_t> examples = {-1,
                                   -10,
                                   -24,
                                   -25,
                                   -300,
                                   -30000,
                                   -300 * 1000,
                                   -1000 * 1000,
                                   -1000 * 1000 * 1000,
                                   -5 * 1000 * 1000 * 1000,
                                   std::numeric_limits<int64_t>::min()};
  for (int64_t example : examples) {
    SCOPED_TRACE(base::StringPrintf("example %ld", example));
    std::vector<uint8_t> encoded;
    EncodeNegative(example, &encoded);
    int64_t decoded = 0;
    span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
    EXPECT_TRUE(DecodeNegative(&encoded_bytes, &decoded));
    EXPECT_EQ(example, decoded);
    EXPECT_TRUE(encoded_bytes.empty());
  }
}
}  // namespace internal

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

TEST(EncodeDecodeUTF16StringTest, ErrorCases) {
  struct TestCase {
    std::vector<uint8_t> data;
    std::string msg;
  };
  std::vector<TestCase> tests{
      {TestCase{{0}, "we require major type 2 (but it's 0)"},
       TestCase{{2 << 5 | 1, 'a'},
                "length must be divisible by 2 (but it's 1)"},
       TestCase{{2 << 5 | 29}, "additional info = 29 isn't recognized"}}};
  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.msg);
    std::vector<uint16_t> decoded;
    span<uint8_t> encoded_bytes(&test.data[0], test.data.size());
    EXPECT_FALSE(DecodeUTF16String(&encoded_bytes, &decoded));
    EXPECT_TRUE(decoded.empty());
    EXPECT_EQ(test.data.size(), size_t(encoded_bytes.size()));
  }
}

TEST(EncodeDecodeDoubleTest, RoundtripsWikipediaExample) {
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format
  // provides the example of a hex representation 3FD5 5555 5555 5555, which
  // approximates 1/3.

  std::vector<uint8_t> encoded;
  EncodeDouble(1.0 / 3, &encoded);
  // first three bits: major type = 7; remaining five bits: additional info =
  // value 27. This is followed by 8 bytes of payload (which match Wikipedia).
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 9>{
          {7 << 5 | 27, 0x3f, 0xd5, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}}));

  // Now the reverse direction: decode the encoded empty string and store it
  // into decoded.
  double decoded = 0;
  span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
  EXPECT_TRUE(DecodeDouble(&encoded_bytes, &decoded));
  EXPECT_THAT(decoded, testing::DoubleEq(1.0 / 3));
  EXPECT_TRUE(encoded_bytes.empty());
}

TEST(EncodeDecodeDoubleTest, RoundtripsAdditionalExamples) {
  std::vector<double> examples = {0.0,
                                  1.0,
                                  -1.0,
                                  3.1415,
                                  std::numeric_limits<double>::min(),
                                  std::numeric_limits<double>::max(),
                                  std::numeric_limits<double>::infinity(),
                                  std::numeric_limits<double>::quiet_NaN()};
  for (double example : examples) {
    SCOPED_TRACE(base::StringPrintf("example %lf", example));
    std::vector<uint8_t> encoded;
    EncodeDouble(example, &encoded);
    double decoded = 0;
    span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
    EXPECT_TRUE(DecodeDouble(&encoded_bytes, &decoded));
    if (std::isnan(example)) {
      EXPECT_TRUE(std::isnan(decoded));
    } else {
      EXPECT_THAT(decoded, testing::DoubleEq(example));
    }
    EXPECT_TRUE(encoded_bytes.empty());
  }
}

void EncodeAsciiStringForTest(const std::string& key,
                              std::vector<uint8_t>* out) {
  // TODO(johannes): Later, we'll want to support utf8 strings for such keys.
  // But for now, utf16 is all we've got.
  std::vector<uint16_t> utf16;
  for (char c : key) utf16.push_back(static_cast<uint8_t>(c));
  EncodeUTF16String(span<uint16_t>(utf16.data(), utf16.size()), out);
}

TEST(JsonToCborConversion, Encoding) {
  // Hits all the cases except error in JsonParserHandler.
  std::string json = R"raw({
     "string": "Hello, \ud83c\udf0e.",
     "double": 3.1415,
     "int": 1,
     "negative int": -1,
     "bool": true,
     "null": null,
     "array": [1,2,3]
  })raw";
  std::vector<uint8_t> out;
  bool error;
  std::unique_ptr<JsonParserHandler> encoder =
      NewJsonToBinaryEncoder(&out, &error);
  span<uint8_t> ascii_in(reinterpret_cast<const uint8_t*>(json.data()),
                         json.size());
  parseJSONChars(GetLinuxDevPlatform(), ascii_in, encoder.get());
  std::vector<uint8_t> expected;
  expected.push_back(0xbf);  // indef length map start
  EncodeAsciiStringForTest("string", &expected);
  // This is followed by the encoded string for "Hello, 🌎."
  // So, it's the same bytes that we tested above in
  // EncodeDecodeUTF16StringTest.RoundtripsHelloWorld.
  expected.push_back(/*major type=*/2 << 5 | /*additional info=*/20);
  for (uint8_t ch : std::array<uint8_t, 20>{
           {'H', 0, 'e', 0, 'l',  0,    'l',  0,    'o', 0,
            ',', 0, ' ', 0, 0x3c, 0xd8, 0x0e, 0xdf, '.', 0}})
    expected.push_back(ch);
  EncodeAsciiStringForTest("double", &expected);
  EncodeDouble(3.1415, &expected);
  EncodeAsciiStringForTest("int", &expected);
  EncodeUnsigned(1, &expected);
  EncodeAsciiStringForTest("negative int", &expected);
  internal::EncodeNegative(-1, &expected);
  EncodeAsciiStringForTest("bool", &expected);
  expected.push_back(7 << 5 | 21);  // RFC 7049 Section 2.3, Table 2: true
  EncodeAsciiStringForTest("null", &expected);
  expected.push_back(7 << 5 | 22);  // RFC 7049 Section 2.3, Table 2: null
  EncodeAsciiStringForTest("array", &expected);
  expected.push_back(0x9f);  // RFC 7049 Section 2.2.1, indef length array start
  expected.push_back(1);     // Three UNSIGNED values (easy since Major Type 0)
  expected.push_back(2);
  expected.push_back(3);
  expected.push_back(0xff);  // End indef length array
  expected.push_back(0xff);  // End indef length map
  EXPECT_THAT(out, ElementsAreArray(expected));
}
}  // namespace inspector_protocol
