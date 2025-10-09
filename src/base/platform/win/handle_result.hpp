/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_PLATFORM_WIN_HANDLE_RESULT_HPP
#define SRC_BASE_PLATFORM_WIN_HANDLE_RESULT_HPP

#include <stdarg.h>
#include <stdio.h>

#include "src/base/platform/win/lean_windows.hpp"

namespace skity {

static const size_t kBufferSize = 2048;

void HandleResult(const char* file, unsigned long line, HRESULT hr,  // NOLINT
                  const char* msg);

template <typename T>
inline void ignore_unused_variable(const T&) {}

#ifdef NDEBUG
#define HANDLE_RESULT(_hr, _msg) ignore_unused_variable(_hr)
#else
#define HANDLE_RESULT(_hr, _msg) HandleResult(__FILE__, __LINE__, _hr, _msg)
#endif

#define HR_GENERAL(_ex, _msg, _ret) \
  do {                              \
    HRESULT _hr = _ex;              \
    if (FAILED(_hr)) {              \
      HANDLE_RESULT(_hr, _msg);     \
      return _ret;                  \
    }                               \
  } while (false)

//@{
/**
These macros are for reporting HRESULT errors.
The expression will be evaluated.
If the resulting HRESULT SUCCEEDED then execution will continue normally.
If the HRESULT FAILED then the macro will return from the current function.
In variants ending with 'M' the given message will be traced when FAILED.
The HR variants will return the HRESULT when FAILED.
The HRB variants will return false when FAILED.
The HRN variants will return nullptr when FAILED.
The HRV variants will simply return when FAILED.
The HRZ variants will return 0 when FAILED.
*/
#define HR(ex) HR_GENERAL(ex, nullptr, _hr)
#define HRM(ex, msg) HR_GENERAL(ex, msg, _hr)

#define HRB(ex) HR_GENERAL(ex, nullptr, false)
#define HRBM(ex, msg) HR_GENERAL(ex, msg, false)

#define HRN(ex) HR_GENERAL(ex, nullptr, nullptr)
#define HRNM(ex, msg) HR_GENERAL(ex, msg, nullptr)

#define HRV(ex) HR_GENERAL(ex, nullptr, )
#define HRVM(ex, msg) HR_GENERAL(ex, msg, )

#define HRZ(ex) HR_GENERAL(ex, nullptr, 0)
#define HRZM(ex, msg) HR_GENERAL(ex, msg, 0)
//@}

}  // namespace skity

#endif  // SRC_BASE_PLATFORM_WIN_HANDLE_RESULT_HPP
