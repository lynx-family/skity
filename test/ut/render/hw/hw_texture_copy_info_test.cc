// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "src/render/hw/hw_texture_copy_utils.hpp"

TEST(HWTextureCopyInfo, BuildDstTextureCopyInfoReturnsNulloptForEmptyBounds) {
  auto info = skity::hw_texture_copy_utils::BuildDstTextureCopyInfo(
      skity::Rect{}, 100, 80, false);

  EXPECT_FALSE(info.has_value());
}

TEST(HWTextureCopyInfo, BuildDstTextureCopyInfoClampsAndRoundsBounds) {
  auto info = skity::hw_texture_copy_utils::BuildDstTextureCopyInfo(
      skity::Rect::MakeLTRB(-3.4f, 10.2f, 102.8f, 20.9f), 100, 80, false);

  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->copy_region.x, 0u);
  EXPECT_EQ(info->copy_region.y, 10u);
  EXPECT_EQ(info->copy_region.width, 100u);
  EXPECT_EQ(info->copy_region.height, 11u);
  EXPECT_FLOAT_EQ(info->uv_mapping.x, 1.f / 100.f);
  EXPECT_FLOAT_EQ(info->uv_mapping.y, 1.f / 11.f);
  EXPECT_FLOAT_EQ(info->uv_mapping.z, 0.f);
  EXPECT_FLOAT_EQ(info->uv_mapping.w, -10.f / 11.f);
}

TEST(HWTextureCopyInfo, BuildDstUVMappingRespectsBottomLeftOrigin) {
  auto uv = skity::hw_texture_copy_utils::BuildDstUVMapping(
      skity::Rect::MakeLTRB(10.f, 20.f, 30.f, 50.f), 80, true);

  EXPECT_FLOAT_EQ(uv.x, 1.f / 20.f);
  EXPECT_FLOAT_EQ(uv.y, 1.f / 30.f);
  EXPECT_FLOAT_EQ(uv.z, -10.f / 20.f);
  EXPECT_FLOAT_EQ(uv.w, -30.f / 30.f);
}
