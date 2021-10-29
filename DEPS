# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# ! DO NOT ROLL THIS FILE INTO CHROMIUM (or other repositories). !
# ! It's only useful for the standalone configuration in         !
# ! https://chromium.googlesource.com/deps/inspector_protocol/   !
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#
# This file configures gclient, a tool that installs dependencies
# at particular versions into this source tree. See
# https://chromium.googlesource.com/chromium/tools/depot_tools.git
# To fetch these dependencies, run "gclient sync". The fetch
# command (from depot_tools) will also run gclient sync.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'gn_version': 'git_revision:79c6c1b1a24c46df5a773cc61604bb5051ca6cf4',
  'libcxx_revision': '79a2e924d96e2fc1e4b937c42efd08898fa472d7',
  'libcxxabi_revision': '1876d9993041bd6960d3abd6dfe5a94acd3a9703',
  'libunwind_revision': '05a4a0312ecc98ac5fbda3823302bc96e47e9ab1',
  'clang_format_revision': '99876cacf78329e5f99c244dbe42ccd1654517a0',
}

# The keys in this dictionary define where the external dependencies (values)
# will be mapped into this gclient. The root of the gclient will
# be the parent directory of the directory in which this DEPS file is.
deps = {
  # gn (the build tool) and clang-format.
  'src/buildtools/clang_format/script':
    Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@' +
    Var('clang_format_revision'),
  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },
  'src/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'src/buildtools/third_party/libc++/trunk':
      Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/libcxx.git' + '@' +
      Var('libcxx_revision'),
  'src/buildtools/third_party/libc++abi/trunk':
      Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' +
      Var('libcxxabi_revision'),
  'src/buildtools/third_party/libunwind/trunk':
      Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/libunwind.git' + '@' +
      Var('libunwind_revision'),
  'src/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },

  # The toolchain definitions (clang C++ compiler etc.)
  'src/third_party/mini_chromium/mini_chromium':
      Var('chromium_git') + '/chromium/mini_chromium@' +
      '461b416dbe5f40a060ee08764b4986a949da6e6e',
  # For writing unittests.
  'src/third_party/gtest/gtest':
      Var('chromium_git') + '/external/github.com/google/googletest@' +
      '16f637fbf4ffc3f7a01fa4eceb7906634565242f',
}

hooks = [
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '-s',
      'src/buildtools/linux64/clang-format.sha1',
    ],
  },
]
recursedeps = ['mini_chromium']
