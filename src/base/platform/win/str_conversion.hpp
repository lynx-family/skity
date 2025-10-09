/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_PLATFORM_WIN_STR_CONVERSION_HPP
#define SRC_BASE_PLATFORM_WIN_STR_CONVERSION_HPP

// clang-format off
#include "src/base/platform/win/lean_windows.hpp"
#include "src/base/platform/win/handle_result.hpp"
// clang-format on

#include <string>

namespace skity {

// UTF-8 std::string <-> UTF-16 std::wstring
class StrConversion {
 public:
  static HRESULT StringToWideString(const std::string& str,
                                    std::wstring* wstr) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0) - 1;
    if (wlen == 0) {
      if (str.size() == 0) {
        return S_OK;
      }
      HRM(HRESULT_FROM_WIN32(GetLastError()),
          "Could not get length for utf-8 to wchar conversion.");
    }

    wstr->resize(wlen);
    wlen =
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &(*wstr)[0], wlen) - 1;
    if (0 == wlen) {
      HRM(HRESULT_FROM_WIN32(GetLastError()),
          "Could not convert utf-8 to wchar.");
    }
    return S_OK;
  }

  static HRESULT WideStringToString(const std::wstring& wstr,
                                    std::string* str) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
                                  nullptr, nullptr) -
              1;
    if (0 == len) {
      if (wstr.size() == 0) {
        return S_OK;
      }
      HRM(HRESULT_FROM_WIN32(GetLastError()),
          "Could not get length for wchar to utf-8 conversion.");
    }

    str->resize(len);
    len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &(*str)[0], len,
                              nullptr, nullptr) -
          1;
    if (0 == len) {
      HRM(HRESULT_FROM_WIN32(GetLastError()),
          "Could not convert wchar to utf-8.");
    }
    return S_OK;
  }
};

}  // namespace skity

#endif  // SRC_BASE_PLATFORM_WIN_STR_CONVERSION_HPP
