// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_stack_blur.hpp"

#include <array>
#include <cstring>
#include <skity/graphic/bitmap.hpp>
#include <vector>

namespace skity {

SWStackBlur::SWStackBlur(Bitmap* src, Bitmap* dst, int32_t blur_radius)
    : src_(src), dst_(dst), blur_radius_(std::min(blur_radius, 254)) {}

void SWStackBlur::Blur() {
  uint8_t* pixels = reinterpret_cast<uint8_t*>(src_->GetPixelAddr());
  uint8_t* dst_pixels = reinterpret_cast<uint8_t*>(dst_->GetPixelAddr());

  if (blur_radius_ <= 1) {
    auto bytes = std::min(src_->GetPixmap()->RowBytes() * src_->Height(),
                          dst_->GetPixmap()->RowBytes() * dst_->Height());
    std::memcpy(dst_pixels, pixels, bytes);
    return;
  }

  int32_t div = 2 * blur_radius_ + 1;
  int32_t width = src_->Width();
  int32_t height = src_->Height();
  int32_t width_minus_1 = width - 1;
  int32_t height_minus_1 = height - 1;
  int32_t radius_plus_1 = blur_radius_ + 1;
  int32_t sum_factor = radius_plus_1 * (radius_plus_1 + 1) / 2;

  int32_t stack_start = 0;
  int32_t stack_end = radius_plus_1;

  std::vector<glm::u64vec4> stack(div);

  int32_t stack_in = 0;
  int32_t stack_out = 0;

  int32_t yw = 0;
  int32_t yi = 0;

  int32_t mul_sum = GetMulSum(blur_radius_);
  int32_t shr_sum = GetShrSum(blur_radius_);

  for (int32_t y = 0; y < height; y++) {
    int32_t p_stack = stack_start;

    uint8_t pb = pixels[yi];
    uint8_t pg = pixels[yi + 1];
    uint8_t pr = pixels[yi + 2];
    uint8_t pa = pixels[yi + 3];

    for (int32_t i = 0; i < radius_plus_1; i++) {
      stack[p_stack].r = pr;
      stack[p_stack].g = pg;
      stack[p_stack].b = pb;
      stack[p_stack].a = pa;

      p_stack++;
    }

    glm::u64vec4 in_sum{};
    glm::u64vec4 out_sum{};
    glm::u64vec4 sum{};

    out_sum.r = pr * radius_plus_1;
    out_sum.g = pg * radius_plus_1;
    out_sum.b = pb * radius_plus_1;
    out_sum.a = pa * radius_plus_1;

    sum.r = pr * sum_factor;
    sum.g = pg * sum_factor;
    sum.b = pb * sum_factor;
    sum.a = pa * sum_factor;

    for (int32_t i = 1; i < radius_plus_1; i++) {
      int32_t p = yi + ((width_minus_1 < i ? width_minus_1 : i) << 2);

      int32_t b = pixels[p];
      int32_t g = pixels[p + 1];
      int32_t r = pixels[p + 2];
      int32_t a = pixels[p + 3];

      int32_t rbs = radius_plus_1 - i;

      stack[p_stack].r = r;
      stack[p_stack].g = g;
      stack[p_stack].b = b;
      stack[p_stack].a = a;

      sum += (stack[p_stack] * uint64_t(rbs));

      in_sum.r += r;
      in_sum.g += g;
      in_sum.b += b;
      in_sum.a += a;

      p_stack++;
    }

    stack_in = stack_start;
    stack_out = stack_end;

    for (int32_t x = 0; x < width; x++) {
      dst_pixels[yi] = (sum.b * mul_sum) >> shr_sum;
      dst_pixels[yi + 1] = (sum.g * mul_sum) >> shr_sum;
      dst_pixels[yi + 2] = (sum.r * mul_sum) >> shr_sum;
      dst_pixels[yi + 3] = (sum.a * mul_sum) >> shr_sum;

      sum -= out_sum;

      out_sum -= stack[stack_in];

      int32_t p = x + radius_plus_1;
      p = (yw + (p < width_minus_1 ? p : width_minus_1)) << 2;

      stack[stack_in].b = pixels[p];
      stack[stack_in].g = pixels[p + 1];
      stack[stack_in].r = pixels[p + 2];
      stack[stack_in].a = pixels[p + 3];

      in_sum += stack[stack_in];

      sum += in_sum;

      stack_in++;

      if (stack_in >= static_cast<int32_t>(stack.size())) {
        stack_in = 0;
      }

      auto r = stack[stack_out].r;
      auto g = stack[stack_out].g;
      auto b = stack[stack_out].b;
      auto a = stack[stack_out].a;

      out_sum.r += r;
      out_sum.g += g;
      out_sum.b += b;
      out_sum.a += a;

      in_sum.r -= r;
      in_sum.g -= g;
      in_sum.b -= b;
      in_sum.a -= a;

      stack_out++;
      if (stack_out >= static_cast<int32_t>(stack.size())) {
        stack_out = 0;
      }

      yi += 4;
    }

    yw += width;
  }

  for (int32_t x = 0; x < width; x++) {
    yi = x << 2;

    auto pb = dst_pixels[yi];
    auto pg = dst_pixels[yi + 1];
    auto pr = dst_pixels[yi + 2];
    auto pa = dst_pixels[yi + 3];

    glm::u64vec4 in_sum{};
    glm::u64vec4 out_sum{};
    glm::u64vec4 sum{};

    out_sum.r = pr * radius_plus_1;
    out_sum.g = pg * radius_plus_1;
    out_sum.b = pg * radius_plus_1;
    out_sum.a = pg * radius_plus_1;

    sum.r = pr * sum_factor;
    sum.g = pg * sum_factor;
    sum.b = pb * sum_factor;
    sum.a = pa * sum_factor;

    int32_t p_stack = stack_start;

    for (int32_t i = 0; i < radius_plus_1; i++) {
      stack[p_stack].r = pr;
      stack[p_stack].g = pg;
      stack[p_stack].b = pb;
      stack[p_stack].a = pa;

      p_stack++;
    }

    int32_t yp = width;

    for (int32_t i = 1; i <= blur_radius_; i++) {
      yi = (yp + x) << 2;

      int32_t rbs = radius_plus_1 - i;

      stack[p_stack].b = (pb = dst_pixels[yi]);
      stack[p_stack].g = (pg = dst_pixels[yi + 1]);
      stack[p_stack].r = (pr = dst_pixels[yi + 2]);
      stack[p_stack].a = (pa = dst_pixels[yi + 3]);

      sum.r += pr * rbs;
      sum.g += pg * rbs;
      sum.b += pb * rbs;
      sum.a += pa * rbs;

      in_sum.r += pr;
      in_sum.g += pg;
      in_sum.b += pb;
      in_sum.a += pa;

      p_stack++;

      if (i < height_minus_1) {
        yp += width;
      }
    }

    yi = x;
    stack_in = stack_start;
    stack_out = stack_end;

    for (int32_t y = 0; y < height; y++) {
      int32_t p = yi << 2;

      dst_pixels[p] = (sum.b * mul_sum) >> shr_sum;
      dst_pixels[p + 1] = (sum.g * mul_sum) >> shr_sum;
      dst_pixels[p + 2] = (sum.r * mul_sum) >> shr_sum;
      dst_pixels[p + 3] = (sum.a * mul_sum) >> shr_sum;

      sum -= out_sum;

      out_sum -= stack[stack_in];

      p = (x +
           (((p = y + radius_plus_1) < height_minus_1 ? p : height_minus_1) *
            width))
          << 2;

      stack[stack_in].b = dst_pixels[p];
      stack[stack_in].g = dst_pixels[p + 1];
      stack[stack_in].r = dst_pixels[p + 2];
      stack[stack_in].a = dst_pixels[p + 3];

      in_sum += stack[stack_in];

      sum += in_sum;

      stack_in++;
      if (stack_in >= static_cast<int32_t>(stack.size())) {
        stack_in = 0;
      }

      pr = stack[stack_out].r;
      pg = stack[stack_out].g;
      pb = stack[stack_out].b;
      pa = stack[stack_out].a;

      out_sum.r += pr;
      out_sum.g += pg;
      out_sum.b += pb;
      out_sum.a += pa;

      in_sum.r -= pr;
      in_sum.g -= pg;
      in_sum.b -= pb;
      in_sum.a -= pa;

      stack_out++;
      if (stack_out >= static_cast<int32_t>(stack.size())) {
        stack_out = 0;
      }

      yi += width;
    }
  }
}

int32_t SWStackBlur::GetMulSum(int32_t radius) {
  static std::array<int32_t, 255> stack_blur_mul = {
      512, 512, 456, 512, 328, 456, 335, 512, 405, 328, 271, 456, 388, 335, 292,
      512, 454, 405, 364, 328, 298, 271, 496, 456, 420, 388, 360, 335, 312, 292,
      273, 512, 482, 454, 428, 405, 383, 364, 345, 328, 312, 298, 284, 271, 259,
      496, 475, 456, 437, 420, 404, 388, 374, 360, 347, 335, 323, 312, 302, 292,
      282, 273, 265, 512, 497, 482, 468, 454, 441, 428, 417, 405, 394, 383, 373,
      364, 354, 345, 337, 328, 320, 312, 305, 298, 291, 284, 278, 271, 265, 259,
      507, 496, 485, 475, 465, 456, 446, 437, 428, 420, 412, 404, 396, 388, 381,
      374, 367, 360, 354, 347, 341, 335, 329, 323, 318, 312, 307, 302, 297, 292,
      287, 282, 278, 273, 269, 265, 261, 512, 505, 497, 489, 482, 475, 468, 461,
      454, 447, 441, 435, 428, 422, 417, 411, 405, 399, 394, 389, 383, 378, 373,
      368, 364, 359, 354, 350, 345, 341, 337, 332, 328, 324, 320, 316, 312, 309,
      305, 301, 298, 294, 291, 287, 284, 281, 278, 274, 271, 268, 265, 262, 259,
      257, 507, 501, 496, 491, 485, 480, 475, 470, 465, 460, 456, 451, 446, 442,
      437, 433, 428, 424, 420, 416, 412, 408, 404, 400, 396, 392, 388, 385, 381,
      377, 374, 370, 367, 363, 360, 357, 354, 350, 347, 344, 341, 338, 335, 332,
      329, 326, 323, 320, 318, 315, 312, 310, 307, 304, 302, 299, 297, 294, 292,
      289, 287, 285, 282, 280, 278, 275, 273, 271, 269, 267, 265, 263, 261, 259,
  };

  if (radius < 255) {
    return stack_blur_mul[radius];
  } else {
    return 0;
  }
}

int32_t SWStackBlur::GetShrSum(int32_t radius) {
  static std::array<int32_t, 255> stack_blur_shr = {
      9,  11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17,
      17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19,
      19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20,
      20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21,
      21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
      21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
      22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
      22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  };

  if (radius < 255) {
    return stack_blur_shr[radius];
  } else {
    return 0;
  }
}

}  // namespace skity
