// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_parser.h"

#include <cassert>
#include <limits>
#include <string>

namespace inspector_protocol {
namespace {
const int kStackLimit = 1000;

enum Token {
  ObjectBegin,
  ObjectEnd,
  ArrayBegin,
  ArrayEnd,
  StringLiteral,
  Number,
  BoolTrue,
  BoolFalse,
  NullToken,
  ListSeparator,
  ObjectPairSeparator,
  InvalidToken,
};

const char* const kNullString = "null";
const char* const kTrueString = "true";
const char* const kFalseString = "false";

template <typename Char>
class JsonParser {
 public:
  JsonParser(const SystemDeps* deps, JsonParserHandler* handler)
      : deps_(deps), handler_(handler) {}

  void Parse(const Char* start, size_t length) {
    const Char* end = start + length;
    const Char* tokenEnd;
    ParseValue(start, end, &tokenEnd, 0);
    if (tokenEnd != end) HandleError();
  }

 private:
  bool CharsToDouble(const uint16_t* chars, size_t length, double* result) {
    std::string buffer;
    buffer.reserve(length + 1);
    for (size_t ii = 0; ii < length; ++ii) {
      bool is_ascii = !(chars[ii] & ~0x7F);
      if (!is_ascii) return false;
      buffer.push_back(static_cast<char>(chars[ii]));
    }
    return deps_->StrToD(buffer.c_str(), result);
  }

  bool CharsToDouble(const uint8_t* chars, size_t length, double* result) {
    std::string buffer(reinterpret_cast<const char*>(chars), length);
    return deps_->StrToD(buffer.c_str(), result);
  }

  static bool ParseConstToken(const Char* start, const Char* end,
                              const Char** token_end, const char* token) {
    // |token| is \0 terminated, it's one of the constants at top of the file.
    while (start < end && *token != '\0' && *start++ == *token++) {
    }
    if (*token != '\0') return false;
    *token_end = start;
    return true;
  }

  static bool ReadInt(const Char* start, const Char* end,
                      const Char** token_end, bool allow_leading_zeros) {
    if (start == end) return false;
    bool has_leading_zero = '0' == *start;
    int length = 0;
    while (start < end && '0' <= *start && *start <= '9') {
      ++start;
      ++length;
    }
    if (!length) return false;
    if (!allow_leading_zeros && length > 1 && has_leading_zero) return false;
    *token_end = start;
    return true;
  }

  static bool ParseNumberToken(const Char* start, const Char* end,
                               const Char** token_end) {
    // We just grab the number here. We validate the size in DecodeNumber.
    // According to RFC4627, a valid number is: [minus] int [frac] [exp]
    if (start == end) return false;
    Char c = *start;
    if ('-' == c) ++start;

    if (!ReadInt(start, end, &start, /*allow_leading_zeros=*/false))
      return false;
    if (start == end) {
      *token_end = start;
      return true;
    }

    // Optional fraction part
    c = *start;
    if ('.' == c) {
      ++start;
      if (!ReadInt(start, end, &start, /*allow_leading_zeros=*/true))
        return false;
      if (start == end) {
        *token_end = start;
        return true;
      }
      c = *start;
    }

    // Optional exponent part
    if ('e' == c || 'E' == c) {
      ++start;
      if (start == end) return false;
      c = *start;
      if ('-' == c || '+' == c) {
        ++start;
        if (start == end) return false;
      }
      if (!ReadInt(start, end, &start, /*allow_leading_zeros=*/true))
        return false;
    }

    *token_end = start;
    return true;
  }

  static bool ReadHexDigits(const Char* start, const Char* end,
                            const Char** token_end, int digits) {
    if (end - start < digits) return false;
    for (int i = 0; i < digits; ++i) {
      Char c = *start++;
      if (!(('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
            ('A' <= c && c <= 'F')))
        return false;
    }
    *token_end = start;
    return true;
  }

  static bool ParseStringToken(const Char* start, const Char* end,
                               const Char** token_end) {
    while (start < end) {
      Char c = *start++;
      if ('\\' == c) {
        if (start == end) return false;
        c = *start++;
        // Make sure the escaped char is valid.
        switch (c) {
          case 'x':
            if (!ReadHexDigits(start, end, &start, 2)) return false;
            break;
          case 'u':
            if (!ReadHexDigits(start, end, &start, 4)) return false;
            break;
          case '\\':
          case '/':
          case 'b':
          case 'f':
          case 'n':
          case 'r':
          case 't':
          case 'v':
          case '"':
            break;
          default:
            return false;
        }
      } else if ('"' == c) {
        *token_end = start;
        return true;
      }
    }
    return false;
  }

  static bool SkipComment(const Char* start, const Char* end,
                          const Char** comment_end) {
    if (start == end) return false;

    if (*start != '/' || start + 1 >= end) return false;
    ++start;

    if (*start == '/') {
      // Single line comment, read to newline.
      for (++start; start < end; ++start) {
        if (*start == '\n' || *start == '\r') {
          *comment_end = start + 1;
          return true;
        }
      }
      *comment_end = end;
      // Comment reaches end-of-input, which is fine.
      return true;
    }

    if (*start == '*') {
      Char previous = '\0';
      // Block comment, read until end marker.
      for (++start; start < end; previous = *start++) {
        if (previous == '*' && *start == '/') {
          *comment_end = start + 1;
          return true;
        }
      }
      // Block comment must close before end-of-input.
      return false;
    }

    return false;
  }

  static bool IsSpaceOrNewLine(Char c) {
    // \v = vertial tab; \f = form feed page break.
    return c == ' ' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
  }

  static void SkipWhitespaceAndComments(const Char* start, const Char* end,
                                        const Char** whitespace_end) {
    while (start < end) {
      if (IsSpaceOrNewLine(*start)) {
        ++start;
      } else if (*start == '/') {
        const Char* comment_end;
        if (!SkipComment(start, end, &comment_end)) break;
        start = comment_end;
      } else {
        break;
      }
    }
    *whitespace_end = start;
  }

  static Token ParseToken(const Char* start, const Char* end,
                          const Char** tokenStart, const Char** token_end) {
    SkipWhitespaceAndComments(start, end, tokenStart);
    start = *tokenStart;

    if (start == end) return InvalidToken;

    switch (*start) {
      case 'n':
        if (ParseConstToken(start, end, token_end, kNullString))
          return NullToken;
        break;
      case 't':
        if (ParseConstToken(start, end, token_end, kTrueString))
          return BoolTrue;
        break;
      case 'f':
        if (ParseConstToken(start, end, token_end, kFalseString))
          return BoolFalse;
        break;
      case '[':
        *token_end = start + 1;
        return ArrayBegin;
      case ']':
        *token_end = start + 1;
        return ArrayEnd;
      case ',':
        *token_end = start + 1;
        return ListSeparator;
      case '{':
        *token_end = start + 1;
        return ObjectBegin;
      case '}':
        *token_end = start + 1;
        return ObjectEnd;
      case ':':
        *token_end = start + 1;
        return ObjectPairSeparator;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-':
        if (ParseNumberToken(start, end, token_end)) return Number;
        break;
      case '"':
        if (ParseStringToken(start + 1, end, token_end)) return StringLiteral;
        break;
    }
    return InvalidToken;
  }

  static int HexToInt(Char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    assert(false);  // Unreachable.
    return 0;
  }

  static bool DecodeString(const Char* start, const Char* end,
                           std::vector<uint16_t>* output) {
    if (start == end) return true;
    if (start > end) return false;
    output->reserve(end - start);
    while (start < end) {
      uint16_t c = *start++;
      if ('\\' != c) {
        output->push_back(c);
        continue;
      }
      if (start == end) return false;
      c = *start++;

      if (c == 'x') {
        // \x is not supported.
        return false;
      }

      switch (c) {
        case '"':
        case '/':
        case '\\':
          break;
        case 'b':
          c = '\b';
          break;
        case 'f':
          c = '\f';
          break;
        case 'n':
          c = '\n';
          break;
        case 'r':
          c = '\r';
          break;
        case 't':
          c = '\t';
          break;
        case 'v':
          c = '\v';
          break;
        case 'u':
          c = (HexToInt(*start) << 12) + (HexToInt(*(start + 1)) << 8) +
              (HexToInt(*(start + 2)) << 4) + HexToInt(*(start + 3));
          start += 4;
          break;
        default:
          return false;
      }
      output->push_back(c);
    }
    return true;
  }

  void ParseValue(const Char* start, const Char* end,
                  const Char** value_token_end, int depth) {
    if (depth > kStackLimit) {
      HandleError();
      return;
    }
    const Char* token_start;
    const Char* token_end;
    Token token = ParseToken(start, end, &token_start, &token_end);
    switch (token) {
      case InvalidToken:
        HandleError();
        return;
      case NullToken:
        handler_->HandleNull();
        break;
      case BoolTrue:
        handler_->HandleBool(true);
        break;
      case BoolFalse:
        handler_->HandleBool(false);
        break;
      case Number: {
        double value;
        if (!CharsToDouble(token_start, token_end - token_start, &value)) {
          handler_->HandleError();
          return;
        }
        if (value >= std::numeric_limits<int32_t>::min() &&
            value <= std::numeric_limits<int32_t>::max() &&
            static_cast<int32_t>(value) == value)
          handler_->HandleInt(static_cast<int32_t>(value));
        else
          handler_->HandleDouble(value);
        break;
      }
      case StringLiteral: {
        std::vector<uint16_t> value;
        bool ok = DecodeString(token_start + 1, token_end - 1, &value);
        if (!ok) {
          HandleError();
          return;
        }
        handler_->HandleString(std::move(value));
        break;
      }
      case ArrayBegin: {
        handler_->HandleArrayBegin();
        start = token_end;
        token = ParseToken(start, end, &token_start, &token_end);
        while (token != ArrayEnd) {
          ParseValue(start, end, &token_end, depth + 1);
          if (error_) return;

          // After a list value, we expect a comma or the end of the list.
          start = token_end;
          token = ParseToken(start, end, &token_start, &token_end);
          if (token == ListSeparator) {
            start = token_end;
            token = ParseToken(start, end, &token_start, &token_end);
            if (token == ArrayEnd) {
              HandleError();
              return;
            }
          } else if (token != ArrayEnd) {
            // Unexpected value after list value. Bail out.
            HandleError();
            return;
          }
        }
        if (token != ArrayEnd) {
          HandleError();
          return;
        }
        handler_->HandleArrayEnd();
        break;
      }
      case ObjectBegin: {
        handler_->HandleObjectBegin();
        start = token_end;
        token = ParseToken(start, end, &token_start, &token_end);
        while (token != ObjectEnd) {
          if (token != StringLiteral) {
            HandleError();
            return;
          }
          std::vector<uint16_t> key;
          if (!DecodeString(token_start + 1, token_end - 1, &key)) {
            HandleError();
            return;
          }
          handler_->HandleString(std::move(key));
          start = token_end;

          token = ParseToken(start, end, &token_start, &token_end);
          if (token != ObjectPairSeparator) {
            HandleError();
            return;
          }
          start = token_end;

          ParseValue(start, end, &token_end, depth + 1);
          if (error_) return;
          start = token_end;

          // After a key/value pair, we expect a comma or the end of the
          // object.
          token = ParseToken(start, end, &token_start, &token_end);
          if (token == ListSeparator) {
            start = token_end;
            token = ParseToken(start, end, &token_start, &token_end);
            if (token == ObjectEnd) {
              HandleError();
              return;
            }
          } else if (token != ObjectEnd) {
            // Unexpected value after last object value. Bail out.
            HandleError();
            return;
          }
        }
        if (token != ObjectEnd) {
          HandleError();
          return;
        }
        handler_->HandleObjectEnd();
        break;
      }

      default:
        // We got a token that's not a value.
        HandleError();
        return;
    }

    SkipWhitespaceAndComments(token_end, end, value_token_end);
  }

  void HandleError() {
    if (!error_) {
      handler_->HandleError();
      error_ = true;
    }
  }

  bool error_ = false;
  const SystemDeps* deps_;
  JsonParserHandler* handler_;
};
}  // namespace

void parseJSONChars(const SystemDeps* deps, span<uint8_t> chars,
                    JsonParserHandler* handler) {
  JsonParser<uint8_t> parser(deps, handler);
  parser.Parse(chars.data(), chars.size());
}

void parseJSONChars(const SystemDeps* deps, span<uint16_t> chars,
                    JsonParserHandler* handler) {
  JsonParser<uint16_t> parser(deps, handler);
  parser.Parse(chars.data(), chars.size());
}
}  // namespace inspector_protocol
