// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/darwin/types_darwin.hpp"

#include <dlfcn.h>

#include <array>
#include <mutex>
#include <skity/macros.hpp>

namespace skity {

bool find_dict_CGFloat(CFDictionaryRef dict, CFStringRef name, CGFloat* value) {
  CFNumberRef num;
  return CFDictionaryGetValueIfPresent(dict, name, (const void**)&num) &&
         CFNumberIsFloatType(num) &&
         CFNumberGetValue(num, kCFNumberCGFloatType, value);
}

int ct_weight_to_fontstyle(CGFloat cg_weight) {
  static std::once_flag flag;

  static std::array<CGFloat, 11> s_ns_font_weights{
      -1.00, -0.80, -0.60, -0.40, 0.00, 0.23, 0.30, 0.40, 0.56, 0.62, 1.00};

#ifdef SKITY_MACOS
#define FONT_WEIGHT_PREFIX "NS"
#endif
#ifdef SKITY_IOS
#define FONT_WEIGHT_PREFIX "UI"
#endif
  static constexpr const char* s_ns_font_weight_names[] = {
      FONT_WEIGHT_PREFIX "FontWeightUltraLight",
      FONT_WEIGHT_PREFIX "FontWeightThin",
      FONT_WEIGHT_PREFIX "FontWeightLight",
      FONT_WEIGHT_PREFIX "FontWeightRegular",
      FONT_WEIGHT_PREFIX "FontWeightMedium",
      FONT_WEIGHT_PREFIX "FontWeightSemibold",
      FONT_WEIGHT_PREFIX "FontWeightBold",
      FONT_WEIGHT_PREFIX "FontWeightHeavy",
      FONT_WEIGHT_PREFIX "FontWeightBlack",
  };

  std::call_once(flag, [&]() {
    size_t i = 0;
    s_ns_font_weights[i++] = -1.00;
    for (const char* name : s_ns_font_weight_names) {
      void* nsFontWeightValuePtr = dlsym(RTLD_DEFAULT, name);
      if (nsFontWeightValuePtr) {
        s_ns_font_weights[i++] = *(static_cast<CGFloat*>(nsFontWeightValuePtr));
      } else {
        return;
      }
    }
    s_ns_font_weights[i++] = 1.00;
  });

  for (int32_t i = 0; i < 11; i++) {
    if (std::abs(s_ns_font_weights[i] - cg_weight) <= 0.00001) {
      return i * 100;
    }
  }

  return 400;
}

// Convert the [-0.5, 0.5] CTFontDescriptor width to [0, 10] CSS weight.
int ct_width_to_fontstyle(CGFloat cg_width) {
  cg_width += 0.5;
  cg_width *= 10.0;

  return static_cast<int32_t>(cg_width);
}

void ct_desc_to_font_style(CTFontDescriptorRef desc, FontStyle* style) {
  UniqueCFRef<CFDictionaryRef> ct_traits(
      (CFDictionaryRef)CTFontDescriptorCopyAttribute(desc,
                                                     kCTFontTraitsAttribute));

  if (ct_traits == nullptr) {
    return;
  }

  CGFloat weight = 0.0;
  CGFloat width = 0.0;
  CGFloat slant = 0.0;

  find_dict_CGFloat(ct_traits.get(), kCTFontWeightTrait, &weight);
  find_dict_CGFloat(ct_traits.get(), kCTFontWidthTrait, &width);
  find_dict_CGFloat(ct_traits.get(), kCTFontSlantTrait, &slant);

  *style =
      FontStyle(ct_weight_to_fontstyle(weight), ct_width_to_fontstyle(width),
                slant ? FontStyle::kItalic_Slant : FontStyle::kUpright_Slant);
}

bool find_desc_str(CTFontDescriptorRef desc, CFStringRef name,
                   std::string* value) {
  UniqueCFRef<CFStringRef> ref(
      (CFStringRef)CTFontDescriptorCopyAttribute(desc, name));
  if (!ref) {
    return false;
  }

  *value = CFStringGetCStringPtr(name, kCFStringEncodingUTF8);
  return true;
}

}  // namespace skity
