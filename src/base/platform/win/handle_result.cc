/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/base/platform/win/handle_result.hpp"

#include "src/logging.hpp"

namespace skity {

void HandleResult(const char* file, unsigned long line, HRESULT hr,  // NOLINT
                  const char* msg) {
  if (msg) {
    LOGE("{}\n", msg);
  }
  LOGE("{}({}) : error {}: ", file, line, hr);

  LPSTR error_text = nullptr;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPSTR)&error_text, 0, nullptr);

  if (nullptr == error_text) {
    LOGE("<unknown>\n");
  } else {
    LOGE("{}", error_text);
    LocalFree(error_text);
    error_text = nullptr;
  }
}

}  // namespace skity
