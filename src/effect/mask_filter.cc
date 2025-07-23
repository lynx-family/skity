// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <skity/effect/mask_filter.hpp>

#if defined(SKITY_CPU)
#include <skity/graphic/bitmap.hpp>
#include <skity/render/canvas.hpp>

#include "src/effect/image_filter_base.hpp"
#include "src/effect/mask_filter_priv.hpp"
#include "src/graphic/blend_mode_priv.hpp"
#include "src/graphic/color_priv.hpp"
#include "src/render/sw/sw_stack_blur.hpp"
#endif

namespace skity {

std::shared_ptr<MaskFilter> MaskFilter::MakeBlur(BlurStyle style,
                                                 float radius) {
  if (radius <= 0) {
    return nullptr;
  }
  auto filter = std::make_shared<MaskFilter>();

  filter->style_ = style;
  filter->radius_ = radius;

  return filter;
}

#if defined(SKITY_CPU)
void MaskFilterOnFilter(Canvas* canvas, Bitmap& bitmap,
                        const Rect& filter_bounds, const Paint& paint,
                        MaskFilter* mask_filter) {
  float radius = mask_filter->GetBlurRadius();
  if (mask_filter->GetBlurStyle() == BlurStyle::kNormal) {
    ImageFilterBase::BlurBitmapToCanvas(canvas, bitmap, filter_bounds, paint,
                                        radius, radius);
    return;
  }

  Bitmap filtered_bitmap(bitmap.Width(), bitmap.Height(), kPremul_AlphaType);
  SWStackBlur(&bitmap, &filtered_bitmap, glm::round(radius)).Blur();

  if (mask_filter->GetBlurStyle() == BlurStyle::kSolid) {
    for (size_t y = 0; y < bitmap.Height(); ++y) {
      for (size_t x = 0; x < bitmap.Width(); ++x) {
        auto raw_color = bitmap.GetPixel(x, y);
        auto raw_a = ColorGetA(raw_color);
        if (raw_a > 0) {
          filtered_bitmap.SetPixel(x, y, raw_color);
        }
      }
    }
  } else if (mask_filter->GetBlurStyle() == BlurStyle::kOuter) {
    for (size_t y = 0; y < bitmap.Height(); ++y) {
      for (size_t x = 0; x < bitmap.Width(); ++x) {
        auto blur_a = ColorGetA(filtered_bitmap.GetPixel(x, y));
        auto raw_a = ColorGetA(bitmap.GetPixel(x, y));
        if (raw_a > 0 && raw_a >= blur_a) {
          filtered_bitmap.SetPixel(x, y, Color_TRANSPARENT);
        }
      }
    }
  } else if (mask_filter->GetBlurStyle() == BlurStyle::kInner) {
    constexpr float a_factor = 1.f / 255.f;
    for (size_t y = 0; y < bitmap.Height(); ++y) {
      for (size_t x = 0; x < bitmap.Width(); ++x) {
        auto blur_c = filtered_bitmap.GetPixel(x, y);
        auto blur_a = ColorGetA(blur_c);
        auto raw_c = bitmap.GetPixel(x, y);
        auto raw_a = ColorGetA(raw_c);
        if (raw_a > 0) {
          filtered_bitmap.SetPixel(
              x, y, AlphaMulQ(blur_c, a_factor * raw_a * blur_a));
        } else {
          filtered_bitmap.SetPixel(x, y, Color_TRANSPARENT);
        }
      }
    }
  }
  canvas->DrawImage(Image::MakeImage(filtered_bitmap.GetPixmap()),
                    filter_bounds, &paint);
}
#endif

}  // namespace skity
