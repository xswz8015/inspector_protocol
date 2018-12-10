// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_LINUX_DEV_SYSTEM_DEPS_H_
#define INSPECTOR_PROTOCOL_ENCODING_LINUX_DEV_SYSTEM_DEPS_H_

#include "system_deps.h"

namespace inspector_protocol {
// Returns an instance of the system deps implementation that we're using for
// development on Linux. This is intended and appropriate for tests for this
// package, for now.
SystemDeps* GetLinuxDevSystemDeps();
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_LINUX_DEV_SYSTEM_DEPS_H_
