// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "linux_dev_system_deps.h"

#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>

namespace inspector_protocol {
namespace {
class LinuxDeps : public SystemDeps {
  bool StrToD(const char* str, double* result) const override {
    locale_t new_locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
    char* end;
    *result = strtod_l(str, &end, new_locale);
    freelocale(new_locale);
    return errno != ERANGE && end == str + strlen(str);
  }

  std::unique_ptr<char[]> DToStr(double value) const override {
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    ss << value;
    std::string str = ss.str();
    std::unique_ptr<char[]> result(new char[str.size() + 1]);
    memcpy(result.get(), str.c_str(), str.size() + 1);
    return result;
  }
};
}  // namespace

SystemDeps* GetLinuxDevSystemDeps() {
  static SystemDeps* deps = new LinuxDeps;
  return deps;
}
}  // namespace inspector_protocol
