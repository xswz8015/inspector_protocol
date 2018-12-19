// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_STATUS_H_
#define INSPECTOR_PROTOCOL_ENCODING_STATUS_H_

#include <cstdint>

namespace inspector_protocol {
// Error codes.
enum class Error {
  OK = 0,
  // JSON parsing errors - json_parser.{h,cc}.
  JSON_PARSER_UNPROCESSED_INPUT_REMAINS = 0x01,
  JSON_PARSER_STACK_LIMIT_EXCEEDED = 0x02,
  JSON_PARSER_NO_INPUT = 0x03,
  JSON_PARSER_INVALID_TOKEN = 0x04,
  JSON_PARSER_INVALID_NUMBER = 0x05,
  JSON_PARSER_INVALID_STRING = 0x06,
  JSON_PARSER_UNEXPECTED_ARRAY_END = 0x07,
  JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED = 0x08,
  JSON_PARSER_STRING_LITERAL_EXPECTED = 0x09,
  JSON_PARSER_COLON_EXPECTED = 0x0a,
  JSON_PARSER_UNEXPECTED_OBJECT_END = 0x0b,
  JSON_PARSER_COMMA_OR_OBJECT_END_EXPECTED = 0x0c,
  JSON_PARSER_VALUE_EXPECTED = 0x0d,

  BINARY_ENCODING_NO_INPUT = 0x0e,
  BINARY_ENCODING_INVALID_START_BYTE = 0x0f,
  BINARY_ENCODING_UNEXPECTED_EOF_EXPECTED_VALUE = 0x10,
  BINARY_ENCODING_UNEXPECTED_EOF_IN_ARRAY = 0x11,
  BINARY_ENCODING_UNEXPECTED_EOF_IN_MAP = 0x12,
  BINARY_ENCODING_INVALID_MAP_KEY = 0x14,
  BINARY_ENCODING_STACK_LIMIT_EXCEEDED = 0x15,
  BINARY_ENCODING_UNSUPPORTED_VALUE = 0x16,
  BINARY_ENCODING_INVALID_STRING16 = 0x17,
  BINARY_ENCODING_INVALID_STRING8 = 0x18,
  BINARY_ENCODING_STRING8_MUST_BE_7BIT = 0x19,
  BINARY_ENCODING_INVALID_DOUBLE = 0x1a,
  BINARY_ENCODING_INVALID_SIGNED = 0x1b,
};

// A status value with position that can be copied. The default status
// is OK. Usually, error status values should come with a valid position.
class Status {
 public:
  static constexpr int64_t npos() { return -1; }

  bool ok() const { return error == Error::OK; }

  Error error = Error::OK;
  int64_t pos = npos();
};
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_STATUS_H_
