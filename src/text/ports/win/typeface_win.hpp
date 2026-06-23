// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_WIN_TYPEFACE_WIN_HPP
#define SRC_TEXT_PORTS_WIN_TYPEFACE_WIN_HPP

// clang-format off
#include "src/text/ports/win/dwrite_version.hpp"
#include <dwrite.h>
// clang-format on

#include <memory>
#include <skity/io/data.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <string>
#include <vector>

#include "src/text/ports/win/dwrite_utils.hpp"
#include "src/text/ports/win/scoped_com_ptr.hpp"

namespace skity {

bool HasBitmapStrikes(const ScopedComPtr<IDWriteFont>& font);
bool DWriteFontFaceEqual(IDWriteFontFace* lhs, IDWriteFontFace* rhs);

HRESULT FirstMatchingFontWithoutSimulations(
    const ScopedComPtr<IDWriteFontFamily>& family, DWriteStyle dwStyle,
    ScopedComPtr<IDWriteFont>& font);

FontStyle FontStyleFromDWriteFont(IDWriteFont* font);

std::string CopyLocalizedString(IDWriteLocalizedStrings* strings,
                                const std::wstring& locale_name);

std::shared_ptr<Typeface> MakeDWriteTypeface(
    IDWriteFactory* factory, IDWriteFontFace* font_face, IDWriteFont* font,
    IDWriteFontFamily* font_family, std::wstring locale_name,
    bool prefer_directwrite_post_script_name = false,
    std::string directwrite_post_script_name_override = {},
    std::shared_ptr<Data> source_data = nullptr);

std::shared_ptr<Typeface> MakeDWriteTypefaceFromDWriteFont(
    IDWriteFactory* factory, const std::wstring& locale_name,
    IDWriteFontFace* font_face, IDWriteFont* font,
    IDWriteFontFamily* font_family);

std::shared_ptr<Typeface> MakeDWriteTypefaceFromFileReference(
    IDWriteFactory* factory, const std::wstring& locale_name, const char* path,
    int ttc_index);

std::shared_ptr<Typeface> MakeDWriteTypefaceFromData(
    IDWriteFactory* factory, const std::wstring& locale_name,
    std::shared_ptr<Data> data, int ttc_index);

}  // namespace skity

#endif  // SRC_TEXT_PORTS_WIN_TYPEFACE_WIN_HPP
