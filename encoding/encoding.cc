// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "encoding.h"

namespace inspector_protocol {

static std::string *kEncodedPrefix = new std::string("ENCODED");

void Encode(const std::string &in, std::string *out) {
  out->append(*kEncodedPrefix);
  out->append(in.rbegin(), in.rend());
}

bool Decode(const std::string &in, std::string *out) {
  if (in.compare(0, kEncodedPrefix->size(), *kEncodedPrefix) != 0)
    return false;
  std::string remainder = in.substr(kEncodedPrefix->size());
  out->append(remainder.rbegin(), remainder.rend());
  return true;
}

} // namespace inspector_protocol
