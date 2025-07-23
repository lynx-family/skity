/*
 * Copyright 2024 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/android/ndk_font_api.hpp"

#include <android/api-level.h>
#include <dlfcn.h>

#include <memory>

namespace skity {

SystemFont::SystemFont(const NdkFontAPI& api, AFont* font)
    : font_api_(api), font_(font) {}

SystemFont::SystemFont(SystemFont&& other)
    : font_api_(other.font_api_), font_(other.font_) {
  other.font_ = nullptr;
}

SystemFont& SystemFont::operator=(SystemFont&& other) {
  if (this->font_) {
    font_api_.AFont_close(this->font_);
  }
  this->font_ = other.font_;
  other.font_ = nullptr;
  return *this;
}

SystemFont::~SystemFont() {
  if (font_) {
    font_api_.AFont_close(font_);
  }
}

SystemFontIterator::SystemFontIterator(const NdkFontAPI& api)
    : font_api_(api), iterator_(font_api_.ASystemFontIterator_open()) {}

SystemFontIterator::~SystemFontIterator() {
  font_api_.ASystemFontIterator_close(iterator_);
}

#if __ANDROID_API__ >= __ANDROID_API_Q__
NdkFontAPI* NdkFontAPI::GetNdkFontAPI() {
  static NdkFontAPI font_api = []() {
    NdkFontAPI font_api;
    font_api.ASystemFontIterator_open = ASystemFontIterator_open;
    font_api.ASystemFontIterator_close = ASystemFontIterator_close;
    font_api.ASystemFontIterator_next = ASystemFontIterator_next;

    font_api.AFont_close = AFont_close;
    font_api.AFont_getFontFilePath = AFont_getFontFilePath;
    font_api.AFont_getWeight = AFont_getWeight;
    font_api.AFont_isItalic = AFont_isItalic;
    font_api.AFont_getLocale = AFont_getLocale;
    font_api.AFont_getCollectionIndex = AFont_getCollectionIndex;
    font_api.AFont_getAxisCount = AFont_getAxisCount;
    font_api.AFont_getAxisTag = AFont_getAxisTag;
    font_api.AFont_getAxisValue = AFont_getAxisValue;
    return font_api;
  }();
  return &font_api;
}

#else

template <typename T, T* P>
struct OverloadedFunctionObject {
  template <typename... Args>
  auto operator()(Args&&... args) const
      -> decltype(P(std::forward<Args>(args)...)) {
    return P(std::forward<Args>(args)...);
  }
};

template <auto F>
using FunctionObject =
    OverloadedFunctionObject<std::remove_pointer_t<decltype(F)>, F>;

NdkFontAPI* NdkFontAPI::GetNdkFontAPI() {
  struct OptionalNdkFontAPI : NdkFontAPI {
    bool valid = false;
  };
  static OptionalNdkFontAPI ndkFontAPI = []() {
    using DLHandle = std::unique_ptr<void, FunctionObject<dlclose> >;
    OptionalNdkFontAPI api;

    if (android_get_device_api_level() < __ANDROID_API_Q__) {
      return api;
    }

    DLHandle self(dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL));
    if (!self) {
      return api;
    }

#define SK_DLSYM_ANDROID_FONT_API(NAME)                            \
  do {                                                             \
    *(void**)(&api.NAME) = dlsym(self.get(), #NAME); /** NOLINT */ \
    if (!api.NAME) {                                               \
      return api;                                                  \
    }                                                              \
  } while (0)

    SK_DLSYM_ANDROID_FONT_API(ASystemFontIterator_open);
    SK_DLSYM_ANDROID_FONT_API(ASystemFontIterator_close);
    SK_DLSYM_ANDROID_FONT_API(ASystemFontIterator_next);

    SK_DLSYM_ANDROID_FONT_API(AFont_close);
    SK_DLSYM_ANDROID_FONT_API(AFont_getFontFilePath);
    SK_DLSYM_ANDROID_FONT_API(AFont_getWeight);
    SK_DLSYM_ANDROID_FONT_API(AFont_isItalic);
    SK_DLSYM_ANDROID_FONT_API(AFont_getLocale);
    SK_DLSYM_ANDROID_FONT_API(AFont_getCollectionIndex);
    SK_DLSYM_ANDROID_FONT_API(AFont_getAxisCount);
    SK_DLSYM_ANDROID_FONT_API(AFont_getAxisTag);
    SK_DLSYM_ANDROID_FONT_API(AFont_getAxisValue);

#undef SK_DLSYM_ANDROID_FONT_API

    api.valid = true;
    return api;
  }();
  return ndkFontAPI.valid ? &ndkFontAPI : nullptr;
}

#endif

}  // namespace skity
