// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <CoreGraphics/CoreGraphics.h>
#include <skity/macros.hpp>

#if defined(SKITY_MACOS)
#import <AppKit/NSFont.h>
#elif defined(SKITY_IOS)
#import <UIKit/UIFont.h>
#endif

namespace skity {

const CGFloat (&get_kit_font_weight_mapping())[11] {
#if defined(SKITY_MACOS)
  static const CGFloat weights[] = {
      -1.00,
      NSFontWeightUltraLight,
      NSFontWeightThin,
      NSFontWeightLight,
      NSFontWeightRegular,
      NSFontWeightMedium,
      NSFontWeightSemibold,
      NSFontWeightBold,
      NSFontWeightHeavy,
      NSFontWeightBlack,
      1.00,
  };
  return weights;
#elif defined(SKITY_IOS)
  static const CGFloat weights[] = {
      -1.00,
      UIFontWeightUltraLight,
      UIFontWeightThin,
      UIFontWeightLight,
      UIFontWeightRegular,
      UIFontWeightMedium,
      UIFontWeightSemibold,
      UIFontWeightBold,
      UIFontWeightHeavy,
      UIFontWeightBlack,
      1.00,
  };
  return weights;
#else
  static const CGFloat weights[] = {-1.00, -0.80, -0.60, -0.40, 0.00, 0.23,
                                    0.30,  0.40,  0.56,  0.62,  1.00};
  return weights;
#endif
}

}  // namespace skity
