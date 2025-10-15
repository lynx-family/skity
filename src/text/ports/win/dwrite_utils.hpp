/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_WIN_DWRITE_UTILS_HPP
#define SRC_TEXT_PORTS_WIN_DWRITE_UTILS_HPP

// clang-format off
#include "src/text/ports/win/dwrite_version.hpp"
#include "src/base/platform/win/lean_windows.hpp"
#include "src/base/platform/win/handle_result.hpp"
#include <dwrite.h>
// clang-format on

#include <skity/text/font_style.hpp>

namespace skity {

class AutoDWriteTable {
 public:
  AutoDWriteTable(IDWriteFontFace* font_face, UINT32 tag)
      : exists(FALSE), font_face_(font_face) {
    // Any errors are ignored, user must check fExists anyway.
    font_face_->TryGetFontTable(tag, reinterpret_cast<const void**>(&data),
                                &size, &table_context_, &exists);
  }
  ~AutoDWriteTable() {
    if (exists) {
      font_face_->ReleaseFontTable(table_context_);
    }
  }

  const uint8_t* data;
  UINT32 size;
  BOOL exists;

 private:
  // Borrowed reference, the user must ensure the fontFace stays alive.
  IDWriteFontFace* font_face_;
  void* table_context_;
};

struct DWriteStyle {
  explicit DWriteStyle(const FontStyle& pattern) {
    weight = (DWRITE_FONT_WEIGHT)pattern.weight();
    width = (DWRITE_FONT_STRETCH)pattern.width();
    switch (pattern.slant()) {
      case FontStyle::kUpright_Slant:
        slant = DWRITE_FONT_STYLE_NORMAL;
        break;
      case FontStyle::kItalic_Slant:
        slant = DWRITE_FONT_STYLE_ITALIC;
        break;
      case FontStyle::kOblique_Slant:
        slant = DWRITE_FONT_STYLE_OBLIQUE;
        break;
      default:
        break;
    }
  }
  DWRITE_FONT_WEIGHT weight;
  DWRITE_FONT_STRETCH width;
  DWRITE_FONT_STYLE slant;
};

// It seems this is not related to DirectWrite.
template <typename PROC_TYPE>
HRESULT LoadWinProc(PROC_TYPE* proc, const wchar_t* module_name, uint64_t flag,
                    const char* func_name) {
  *proc = reinterpret_cast<PROC_TYPE>(
      GetProcAddress(LoadLibraryExW(module_name, NULL, flag), func_name));
  if (!*proc) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    if (!IS_ERROR(hr)) {
      hr = ERROR_PROC_NOT_FOUND;
    }
    return hr;
  }
  return S_OK;
}

}  // namespace skity

#endif  // SRC_TEXT_PORTS_WIN_DWRITE_UTILS_HPP
