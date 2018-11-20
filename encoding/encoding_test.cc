// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "encoding.h"
#include "gtest/gtest.h"
#include <string>

TEST(HelloTest, SimpleRoundtrip) {
  std::string msg = "Hello, world.";
  std::string encoded;
  inspector_protocol::Encode(msg, &encoded);
  std::string decoded;
  EXPECT_TRUE(inspector_protocol::Decode(encoded, &decoded));
  EXPECT_EQ(msg, decoded);
}
