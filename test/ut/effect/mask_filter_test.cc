// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/effect/mask_filter.hpp>
#include <skity/graphic/paint.hpp>

#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {
namespace {

TEST(MaskFilterOpacityTest, IsConservativelyTranslucent) {
  Paint paint;
  paint.SetMaskFilter(MaskFilter::MakeBlur(kNormal, 2.f));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

}  // namespace
}  // namespace skity
