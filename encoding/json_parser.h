// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
#define INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_

#include <cstdint>
#include <vector>
#include "span.h"

namespace inspector_protocol {
// Client code must provide an instance. Implementation should delegate
// to whatever is appropriate.
class SystemDeps {
 public:
  // Parses |str| into |result|. Returns false iff there are
  // leftover characters or parsing errors.
  virtual bool StrToD(const char* str, double* result) const = 0;
};

// Handlers for JSON parser events.
class JsonHandler {
 public:
  virtual void HandleObjectBegin() = 0;
  virtual void HandleObjectEnd() = 0;
  virtual void HandleArrayBegin() = 0;
  virtual void HandleArrayEnd() = 0;
  // TODO(johannes): Support utf8 (requires utf16->utf8 conversion
  // internally, including handling mismatched surrogate pairs).
  virtual void HandleString(std::vector<uint16_t> chars) = 0;
  virtual void HandleDouble(double value) = 0;
  virtual void HandleInt(int32_t value) = 0;
  virtual void HandleBool(bool value) = 0;
  virtual void HandleNull() = 0;

  // The parser may send one error even after other events have already
  // been received. Client code is reponsible to then discard the
  // already processed events.
  virtual void HandleError() = 0;
};

// JSON parsing routines.
void parseJSONChars(const SystemDeps* deps, span<uint8_t> chars,
                    JsonHandler* handler);
void parseJSONChars(const SystemDeps* deps, span<uint16_t> chars,
                    JsonHandler* handler);
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
