// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_std_string_writer.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "linux_dev_platform.h"

namespace inspector_protocol {
std::vector<uint16_t> UTF16String(const std::string& utf8) {
  base::string16 string16 = base::UTF8ToUTF16(utf8);
  return std::vector<uint16_t>(string16.data(),
                               string16.data() + string16.size());
}

TEST(JsonStdStringWriterTest, HelloWorld) {
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  writer->HandleObjectBegin();
  writer->HandleString(UTF16String("msg1"));
  writer->HandleString(UTF16String("Hello, 🌎."));
  writer->HandleString(UTF16String("msg2"));
  writer->HandleString(UTF16String("\\\b\r\n\t\f\""));
  writer->HandleString(UTF16String("nested"));
  writer->HandleObjectBegin();
  writer->HandleString(UTF16String("double"));
  writer->HandleDouble(3.1415);
  writer->HandleString(UTF16String("int"));
  writer->HandleInt(-42);
  writer->HandleString(UTF16String("bool"));
  writer->HandleBool(false);
  writer->HandleString(UTF16String("null"));
  writer->HandleNull();
  writer->HandleObjectEnd();
  writer->HandleString(UTF16String("array"));
  writer->HandleArrayBegin();
  writer->HandleInt(1);
  writer->HandleInt(2);
  writer->HandleInt(3);
  writer->HandleArrayEnd();
  writer->HandleObjectEnd();
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(
      "{\"msg1\":\"Hello, \\ud83c\\udf0e.\","
      "\"msg2\":\"\\\\\\b\\r\\n\\t\\f\\\"\","
      "\"nested\":{\"double\":3.1415,\"int\":-42,"
      "\"bool\":false,\"null\":null},\"array\":[1,2,3]}",
      out);
}

TEST(JsonStdStringWriterTest, HandlesErrors) {
  // When an error is sent via HandleError, it saves it in the provided
  // status and clears the output.
  std::string out;
  Status status;
  std::unique_ptr<JsonParserHandler> writer =
      NewJsonWriter(GetLinuxDevPlatform(), &out, &status);
  writer->HandleObjectBegin();
  writer->HandleString(UTF16String("msg1"));
  writer->HandleError(Status{Error::JSON_PARSER_VALUE_EXPECTED, 42});
  EXPECT_EQ(Error::JSON_PARSER_VALUE_EXPECTED, status.error);
  EXPECT_EQ(42, status.pos);
  EXPECT_EQ("", out);
}
}  // namespace inspector_protocol
