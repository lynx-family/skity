/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/darwin/types_darwin.hpp"

#include <dlfcn.h>

#include <array>
#include <mutex>
#include <skity/macros.hpp>

namespace skity {

namespace {

// From Skia
template <typename S, typename D, typename C>
struct LinearInterpolater {
  struct Mapping {
    S src_val;
    D dst_val;
  };
  constexpr LinearInterpolater(Mapping const mapping[], int mappingCount)
      : fMapping(mapping), fMappingCount(mappingCount) {}

  static D map(S value, S src_min, S src_max, D dst_min, D dst_max) {
    return C()(dst_min + (((value - src_min) * (dst_max - dst_min)) /
                          (src_max - src_min)));
  }

  D map(S val) const {
    // -Inf to [0]
    if (val < fMapping[0].src_val) {
      return fMapping[0].dst_val;
    }

    // Linear from [i] to [i+1]
    for (int i = 0; i < fMappingCount - 1; ++i) {
      if (val < fMapping[i + 1].src_val) {
        return map(val, fMapping[i].src_val, fMapping[i + 1].src_val,
                   fMapping[i].dst_val, fMapping[i + 1].dst_val);
      }
    }

    // From [n] to +Inf
    // if (fcweight < Inf)
    return fMapping[fMappingCount - 1].dst_val;
  }

  Mapping const* fMapping;
  int fMappingCount;
};

struct RoundCGFloatToInt {
  int operator()(CGFloat s) { return s + 0.5; }
};
struct CGFloatIdentity {
  CGFloat operator()(CGFloat s) { return s; }
};

using CTFontWeightMapping = const CGFloat[11];

CTFontWeightMapping& get_font_weight_mapping() {
  // In the event something goes wrong finding the real values, use this
  // mapping.
  static constexpr CGFloat defaultNSFontWeights[] = {
      -1.00, -0.80, -0.60, -0.40, 0.00, 0.23, 0.30, 0.40, 0.56, 0.62, 1.00};

#ifdef SKITY_MACOS
#define SK_KIT_FONT_WEIGHT_PREFIX "NS"
#endif
#ifdef SKITY_IOS
#define SK_KIT_FONT_WEIGHT_PREFIX "UI"
#endif
  static constexpr const char* nsFontWeightNames[] = {
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightUltraLight",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightThin",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightLight",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightRegular",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightMedium",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightSemibold",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightBold",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightHeavy",
      SK_KIT_FONT_WEIGHT_PREFIX "FontWeightBlack",
  };
  static_assert(std::size(nsFontWeightNames) == 9, "");

  static CGFloat nsFontWeights[11];
  static const CGFloat(*selectedNSFontWeights)[11] = &defaultNSFontWeights;
  static std::once_flag flag;
  std::call_once(flag, [&] {
    size_t i = 0;
    nsFontWeights[i++] = -1.00;
    for (const char* nsFontWeightName : nsFontWeightNames) {
      void* nsFontWeightValuePtr = dlsym(RTLD_DEFAULT, nsFontWeightName);
      if (nsFontWeightValuePtr) {
        nsFontWeights[i++] = *(static_cast<CGFloat*>(nsFontWeightValuePtr));
      } else {
        return;
      }
    }
    nsFontWeights[i++] = 1.00;
    selectedNSFontWeights = &nsFontWeights;
  });
  return *selectedNSFontWeights;
}

bool find_dict_CGFloat(CFDictionaryRef dict, CFStringRef name, CGFloat* value) {
  CFNumberRef num;
  return CFDictionaryGetValueIfPresent(dict, name, (const void**)&num) &&
         CFNumberIsFloatType(num) &&
         CFNumberGetValue(num, kCFNumberCGFloatType, value);
}

// Convert the [-1, 1] CTFontDescriptor weight to [0, 1000] fontstyle weight
int ct_weight_to_fontstyle_weight(CGFloat ct_weight) {
  using Interpolator = LinearInterpolater<CGFloat, int, RoundCGFloatToInt>;

  static Interpolator::Mapping weight_mappings[11];
  static std::once_flag flag;
  std::call_once(flag, [&] {
    const CGFloat(&nsFontWeights)[11] = get_font_weight_mapping();
    for (int i = 0; i < 11; ++i) {
      weight_mappings[i].src_val = nsFontWeights[i];
      weight_mappings[i].dst_val = i * 100;
    }
  });
  static constexpr Interpolator interpolator(weight_mappings,
                                             std::size(weight_mappings));

  return interpolator.map(ct_weight);
}

// Convert the [-0.5, 0.5] CTFontDescriptor width to [0, 10] fontstyle width.
int ct_width_to_fontstyle_width(CGFloat ct_width) {
  using Interpolator = LinearInterpolater<CGFloat, int, RoundCGFloatToInt>;

  static constexpr Interpolator::Mapping width_mappings[] = {
      {-0.5, 0},
      {0.5, 10},
  };
  static constexpr Interpolator interpolator(width_mappings,
                                             std::size(width_mappings));
  return interpolator.map(ct_width);
}

bool find_desc_str(CTFontDescriptorRef desc, CFStringRef name,
                   std::string* value) {
  UniqueCFRef<CFStringRef> ref(
      (CFStringRef)CTFontDescriptorCopyAttribute(desc, name));
  if (!ref) {
    return false;
  }

  *value = CFStringGetCStringPtr(ref.get(), kCFStringEncodingUTF8);

  return true;
}

// Convert the [0, 1000] fontstyle weight to [-1, 1] CTFontDescriptor weight
CGFloat fontstyle_weight_to_ct_weight(int fontstyle_weight) {
  using Interpolator = LinearInterpolater<int, CGFloat, CGFloatIdentity>;

  static Interpolator::Mapping weight_mappings[11];
  static std::once_flag flag;
  std::call_once(flag, [&] {
    const CGFloat(&nsFontWeights)[11] = get_font_weight_mapping();
    for (int i = 0; i < 11; ++i) {
      weight_mappings[i].src_val = i * 100;
      weight_mappings[i].dst_val = nsFontWeights[i];
    }
  });
  static constexpr Interpolator interpolator(weight_mappings,
                                             std::size(weight_mappings));

  return interpolator.map(fontstyle_weight);
}

// Convert the [0, 10] fontstyle width to [-0.5, 0.5] CTFontDescriptor width.
CGFloat fontstyle_width_to_ct_width(int fontstyle_width) {
  using Interpolator = LinearInterpolater<int, CGFloat, CGFloatIdentity>;

  static constexpr Interpolator::Mapping width_mappings[] = {
      {0, -0.5},
      {10, 0.5},
  };
  static constexpr Interpolator interpolator(width_mappings,
                                             std::size(width_mappings));
  return interpolator.map(fontstyle_width);
}

}  // namespace

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

  *style = FontStyle(
      ct_weight_to_fontstyle_weight(weight), ct_width_to_fontstyle_width(width),
      slant ? FontStyle::kItalic_Slant : FontStyle::kUpright_Slant);
}

void font_style_to_ct_trait(const FontStyle& style,
                            CFMutableDictionaryRef cf_dict) {
  CGFloat ct_weight = fontstyle_weight_to_ct_weight(style.weight());
  UniqueCFRef<CFNumberRef> cf_font_weight(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ct_weight));
  if (cf_font_weight) {
    CFDictionaryAddValue(cf_dict, kCTFontWeightTrait, cf_font_weight.get());
  }

  CGFloat ct_width = fontstyle_width_to_ct_width(style.width());
  UniqueCFRef<CFNumberRef> cf_font_width(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ct_width));
  if (cf_font_width) {
    CFDictionaryAddValue(cf_dict, kCTFontWidthTrait, cf_font_width.get());
  }

  static const CGFloat kSystemFontItalicSlope = 0.07;
  CGFloat ct_slant =
      style.slant() == FontStyle::kUpright_Slant ? 0 : kSystemFontItalicSlope;
  UniqueCFRef<CFNumberRef> cf_font_slant(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ct_slant));
  if (cf_font_slant) {
    CFDictionaryAddValue(cf_dict, kCTFontSlantTrait, cf_font_slant.get());
  }
}

}  // namespace skity
