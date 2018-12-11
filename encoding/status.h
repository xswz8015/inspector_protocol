// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_STATUS_H_
#define INSPECTOR_PROTOCOL_ENCODING_STATUS_H_

#include <cstdint>

namespace inspector_protocol {
// Error codes.
// Next available: 14
enum class Error {
  OK = 0,
  // JSON parsing errors - json_parser.{h,cc}.
  JSON_PARSER_UNPROCESSED_INPUT_REMAINS = 1,
  JSON_PARSER_STACK_LIMIT_EXCEEDED = 2,
  JSON_PARSER_NO_INPUT = 3,
  JSON_PARSER_INVALID_TOKEN = 4,
  JSON_PARSER_INVALID_NUMBER = 5,
  JSON_PARSER_INVALID_STRING = 6,
  JSON_PARSER_UNEXPECTED_ARRAY_END = 7,
  JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED = 8,
  JSON_PARSER_STRING_LITERAL_EXPECTED = 9,
  JSON_PARSER_COLON_EXPECTED = 10,
  JSON_PARSER_UNEXPECTED_OBJECT_END = 11,
  JSON_PARSER_COMMA_OR_OBJECT_END_EXPECTED = 12,
  JSON_PARSER_VALUE_EXPECTED = 13,
};

// A status value with position that can be copied. The default status
// is OK. Usually, error status values should come with a valid position.
class Status {
 public:
  static constexpr int64_t npos = -1;

  bool ok() const { return error == Error::OK; }

  Error error = Error::OK;
  int64_t pos = Status::npos;
};
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_STATUS_H_
