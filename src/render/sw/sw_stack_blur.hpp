// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_STACK_BLUR_HPP
#define SRC_RENDER_SW_SW_STACK_BLUR_HPP

#include <skity/graphic/color.hpp>

namespace skity {

class Bitmap;

// My implement about
// Mario Kingemann's StackBlur
// http://underdestruction.com/2004/02/25/stackblur-2004/
class SWStackBlur final {
 public:
  SWStackBlur(Bitmap* src, Bitmap* dst, int32_t blur_radius);

  ~SWStackBlur() = default;

  void Blur();

 private:
  static int32_t GetMulSum(int32_t radius);
  static int32_t GetShrSum(int32_t radius);

 private:
  Bitmap* src_;
  Bitmap* dst_;
  int32_t blur_radius_;
};

}  // namespace skity

#endif  // SRC_RENDER_SW_SW_STACK_BLUR_HPP
