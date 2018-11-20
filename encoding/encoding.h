// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This library is experimental and not used within the
// Chromium / V8 / etc. trees yet thus far. Do not depend on anything.
#ifndef INSPECTOR_PROTOCOL_ENCODING_H_
#define INSPECTOR_PROTOCOL_ENCODING_H_

#include <string>

namespace inspector_protocol {

void Encode(const std::string &in, std::string *out);
bool Decode(const std::string &in, std::string *out);

} // namespace inspector_protocol

#endif // INSPECTOR_PROTOCOL_ENCODING_H_
