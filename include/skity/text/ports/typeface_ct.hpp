// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_PORTS_TYPEFACE_CT_HPP
#define INCLUDE_SKITY_TEXT_PORTS_TYPEFACE_CT_HPP

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include <skity/text/typeface.hpp>

namespace skity {

class SKITY_API TypefaceCT {
 public:
  static CTFontRef CTFontFromTypeface(const Typeface* typeface);
  static Typeface* TypefaceFromCTFont(CTFontRef ct_font);
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_PORTS_TYPEFACE_CT_HPP
