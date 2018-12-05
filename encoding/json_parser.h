// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
#define INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_

#include <cstdint>
#include <vector>
#include "json_parser_handler.h"
#include "span.h"

namespace inspector_protocol {
// Client code must provide an instance. Implementation should delegate
// to whatever is appropriate.
class SystemDeps {
 public:
  virtual ~SystemDeps() = default;
  // Parses |str| into |result|. Returns false iff there are
  // leftover characters or parsing errors.
  virtual bool StrToD(const char* str, double* result) const = 0;
};

// JSON parsing routines.
void parseJSONChars(const SystemDeps* deps, span<uint8_t> chars,
                    JsonParserHandler* handler);
void parseJSONChars(const SystemDeps* deps, span<uint16_t> chars,
                    JsonParserHandler* handler);
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
