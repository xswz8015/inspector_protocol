// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_HANDLER_H_
#define INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_HANDLER_H_

#include <cstdint>
#include <vector>

namespace inspector_protocol {
// Handler interface for JSON parser events. See also json_parser.h.
class JsonParserHandler {
 public:
  virtual ~JsonParserHandler() = default;
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
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_HANDLER_H_
