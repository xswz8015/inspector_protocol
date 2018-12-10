// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_parser.h"

#include <iostream>
#include <sstream>
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "linux_dev_system_deps.h"

namespace inspector_protocol {
class Log : public JsonParserHandler {
 public:
  void HandleObjectBegin() override { log_ << "object begin\n"; }
  void HandleObjectEnd() override { log_ << "object end\n"; }
  void HandleArrayBegin() override { log_ << "array begin\n"; }
  void HandleArrayEnd() override { log_ << "array end\n"; }
  void HandleString(std::vector<uint16_t> chars) override {
    base::StringPiece16 foo(reinterpret_cast<const base::char16*>(chars.data()),
                            chars.size());
    log_ << "string: " << base::UTF16ToUTF8(foo) << "\n";
  }
  void HandleDouble(double value) override {
    log_ << "double: " << value << "\n";
  }
  void HandleInt(int32_t value) override { log_ << "int: " << value << "\n"; }
  void HandleBool(bool value) override { log_ << "bool: " << value << "\n"; }
  void HandleNull() override { log_ << "null\n"; }
  void HandleError() override { log_ << "error\n"; }

  std::string str() { return log_.str(); }

 private:
  std::ostringstream log_;
};

class JsonParserTest : public ::testing::Test {
 protected:
  Log log_;
};

TEST_F(JsonParserTest, SimpleDictionary) {
  std::string json = "{\"foo\": 42}";
  parseJSONChars(
      GetLinuxDevSystemDeps(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "int: 42\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, NestedDictionary) {
  std::string json = "{\"foo\": {\"bar\": {\"baz\": 1}, \"bar2\": 2}}";
  parseJSONChars(
      GetLinuxDevSystemDeps(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "object begin\n"
      "string: bar\n"
      "object begin\n"
      "string: baz\n"
      "int: 1\n"
      "object end\n"
      "string: bar2\n"
      "int: 2\n"
      "object end\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Doubles) {
  std::string json = "{\"foo\": 3.1415, \"bar\": 31415e-4}";
  parseJSONChars(
      GetLinuxDevSystemDeps(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "double: 3.1415\n"
      "string: bar\n"
      "double: 3.1415\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Unicode) {
  // Globe character. 0xF0 0x9F 0x8C 0x8E in utf8, 0xD83C 0xDF0E in utf16.
  std::string json = "{\"msg\": \"Hello, \\uD83C\\uDF0E.\"}";
  parseJSONChars(
      GetLinuxDevSystemDeps(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(
      "object begin\n"
      "string: msg\n"
      "string: Hello, 🌎.\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Unicode_ParseUtf16) {
  // Globe character. utf8: 0xF0 0x9F 0x8C 0x8E; utf16: 0xD83C 0xDF0E.
  // Crescent moon character. utf8: 0xF0 0x9F 0x8C 0x99; utf16: 0xD83C 0xDF19.

  // We provide the moon with json escape, but the earth as utf16 input.
  // Either way they arrive as utf8 (after decoding in log_.str()).
  base::string16 json = base::UTF8ToUTF16("{\"space\": \"🌎 \\uD83C\\uDF19.\"}");
  parseJSONChars(GetLinuxDevSystemDeps(),
                 span<uint16_t>(reinterpret_cast<const uint16_t*>(json.data()),
                                json.size()),
                 &log_);
  EXPECT_EQ(
      "object begin\n"
      "string: space\n"
      "string: 🌎 🌙.\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Error) {
  // There's an error because the key bar, a string, is not terminated.
  std::string json = "{\"foo\": 3.1415, \"bar: 31415e-4}";
  parseJSONChars(
      GetLinuxDevSystemDeps(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "double: 3.1415\n"
      "error\n",
      log_.str());
}
}  // namespace inspector_protocol
