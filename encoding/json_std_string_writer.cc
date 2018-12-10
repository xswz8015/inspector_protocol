// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_std_string_writer.h"
#include <cassert>
#include <stack>

namespace inspector_protocol {
namespace {
// Prints |value| to |out| with 4 hex digits, most significant chunk first.
void PrintHex(uint16_t value, std::string* out) {
  for (int ii = 3; ii >= 0; --ii) {
    int four_bits = 0xf & (value >> (4 * ii));
    out->append(1, four_bits + ((four_bits <= 9) ? '0' : ('a' - 10)));
  }
}

// In the writer below, we maintain a stack of State instances.
// It is just enough to emit the appropriate delimiters and brackets
// in JSON.
enum class Container {
  // Used for the top-level, initial state.
  NONE,
  // Inside a JSON object.
  OBJECT,
  // Inside a JSON array.
  ARRAY
};
class State {
 public:
  explicit State(Container container) : container_(container) {}
  void StartElement(std::string* out) {
    assert(container_ != Container::NONE || size_ == 0);
    if (size_ != 0) {
      char delim = (!(size_ & 1) || container_ == Container::ARRAY) ? ',' : ':';
      out->append(1, delim);
    }
    ++size_;
  }
  Container container() const { return container_; }

 private:
  Container container_ = Container::NONE;
  int size_ = 0;
};

// Handler interface for JSON parser events. See also json_parser.h.
class Writer : public JsonParserHandler {
 public:
  Writer(SystemDeps* deps, std::string* out, bool* error)
      : deps_(deps), out_(out), error_(error) {
    *error_ = false;
    state_.emplace(Container::NONE);
  }

  void HandleObjectBegin() override {
    if (*error_) return;
    assert(!state_.empty());
    state_.top().StartElement(out_);
    state_.emplace(Container::OBJECT);
    out_->append("{");
  }

  void HandleObjectEnd() override {
    if (*error_) return;
    assert(state_.size() >= 2 && state_.top().container() == Container::OBJECT);
    state_.pop();
    out_->append("}");
  }

  void HandleArrayBegin() override {
    if (*error_) return;
    state_.top().StartElement(out_);
    state_.emplace(Container::ARRAY);
    out_->append("[");
  }

  void HandleArrayEnd() override {
    if (*error_) return;
    assert(state_.size() >= 2 && state_.top().container() == Container::ARRAY);
    state_.pop();
    out_->append("]");
  }

  void HandleString(std::vector<uint16_t> chars) override {
    if (*error_) return;
    state_.top().StartElement(out_);
    out_->append("\"");
    for (const uint16_t ch : chars) {
      if (ch == '"') {
        out_->append("\\\"");
      } else if (ch == '\\') {
        out_->append("\\\\");
      } else if (ch == '\b') {
        out_->append("\\b");
      } else if (ch == '\f') {
        out_->append("\\f");
      } else if (ch == '\n') {
        out_->append("\\n");
      } else if (ch == '\r') {
        out_->append("\\r");
      } else if (ch == '\t') {
        out_->append("\\t");
      } else if (ch >= 32 && ch <= 126) {
        out_->append(1, ch);
      } else {
        out_->append("\\u");
        PrintHex(ch, out_);
      }
    }
    out_->append("\"");
  }

  void HandleDouble(double value) override {
    if (*error_) return;
    state_.top().StartElement(out_);
    std::unique_ptr<char[]> chars = deps_->DToStr(value);
    out_->append(&chars[0]);
  }

  void HandleInt(int32_t value) override {
    if (*error_) return;
    state_.top().StartElement(out_);
    out_->append(std::to_string(value));
  }

  void HandleBool(bool value) override {
    if (*error_) return;
    state_.top().StartElement(out_);
    out_->append(value ? "true" : "false");
  }

  void HandleNull() override {
    if (*error_) return;
    state_.top().StartElement(out_);
    out_->append("null");
  }

  void HandleError() override { *error_ = true; }

 private:
  SystemDeps* deps_;
  std::string* out_;
  bool* error_;
  std::stack<State> state_;
};
}  // namespace

std::unique_ptr<JsonParserHandler> NewJsonWriter(SystemDeps* deps,
                                                 std::string* out,
                                                 bool* error) {
  return std::make_unique<Writer>(deps, out, error);
}
}  // namespace inspector_protocol
