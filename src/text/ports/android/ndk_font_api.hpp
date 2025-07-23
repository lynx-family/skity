/*
 * Copyright 2024 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_ANDROID_NDK_FONT_API_HPP
#define SRC_TEXT_PORTS_ANDROID_NDK_FONT_API_HPP

#include <cstddef>
#include <cstdint>

namespace skity {

class AFont;
class ASystemFontIterator;

class NdkFontAPI {
 public:
  ASystemFontIterator* (*ASystemFontIterator_open)();
  void (*ASystemFontIterator_close)(ASystemFontIterator*);
  AFont* (*ASystemFontIterator_next)(ASystemFontIterator*);

  void (*AFont_close)(AFont*);
  const char* (*AFont_getFontFilePath)(const AFont*);
  uint16_t (*AFont_getWeight)(const AFont*);
  bool (*AFont_isItalic)(const AFont*);
  const char* (*AFont_getLocale)(const AFont*);
  size_t (*AFont_getCollectionIndex)(const AFont*);
  size_t (*AFont_getAxisCount)(const AFont*);
  uint32_t (*AFont_getAxisTag)(const AFont*, uint32_t axisIndex);
  float (*AFont_getAxisValue)(const AFont*, uint32_t axisIndex);

  static NdkFontAPI* GetNdkFontAPI();
};

class SystemFont {
 public:
  SystemFont(const NdkFontAPI& api, AFont* font);
  SystemFont(SystemFont&& other);
  SystemFont& operator=(SystemFont&& other);

  ~SystemFont();

  explicit operator bool() { return font_; }

  const char* getFontFilePath() const {
    return font_api_.AFont_getFontFilePath(font_);
  }
  uint16_t getWeight() const { return font_api_.AFont_getWeight(font_); }
  bool isItalic() const { return font_api_.AFont_isItalic(font_); }
  const char* getLocale() const { return font_api_.AFont_getLocale(font_); }
  size_t getCollectionIndex() const {
    return font_api_.AFont_getCollectionIndex(font_);
  }
  size_t getAxisCount() const { return font_api_.AFont_getAxisCount(font_); }
  uint32_t getAxisTag(uint32_t index) const {
    return font_api_.AFont_getAxisTag(font_, index);
  }
  float getAxisValue(uint32_t index) const {
    return font_api_.AFont_getAxisValue(font_, index);
  }

 private:
  const NdkFontAPI& font_api_;
  AFont* font_;

  SystemFont(const SystemFont&) = delete;
  SystemFont& operator=(const SystemFont&) = delete;
};

class SystemFontIterator {
 public:
  explicit SystemFontIterator(const NdkFontAPI& api);
  ~SystemFontIterator();

  explicit operator bool() { return iterator_; }

  SystemFont next() {
    return SystemFont(font_api_, font_api_.ASystemFontIterator_next(iterator_));
  }

 private:
  const NdkFontAPI& font_api_;
  ASystemFontIterator* const iterator_;

  SystemFontIterator(const SystemFontIterator&) = delete;
  SystemFontIterator(SystemFontIterator&&) = delete;
  SystemFontIterator& operator=(const SystemFontIterator&) = delete;
  SystemFontIterator& operator=(SystemFontIterator&&) = delete;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_ANDROID_NDK_FONT_API_HPP
