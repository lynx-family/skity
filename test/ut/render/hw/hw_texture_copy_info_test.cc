// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#define private public
#include "src/render/hw/hw_layer.hpp"
#undef private

namespace {

class TestHWLayer final : public skity::HWLayer {
 public:
  TestHWLayer(uint32_t width, uint32_t height)
      : skity::HWLayer(skity::Matrix{}, 1, skity::Rect::MakeWH(width, height),
                       width, height) {}

 private:
  std::shared_ptr<skity::GPURenderPass> OnBeginRenderPass(
      skity::GPUCommandBuffer* cmd, bool force_load) override {
    (void)cmd;
    (void)force_load;
    return nullptr;
  }

  void OnPostDraw(skity::GPURenderPass* render_pass,
                  skity::GPUCommandBuffer* cmd) override {
    (void)render_pass;
    (void)cmd;
  }
};

}  // namespace

TEST(HWTextureCopyInfo, BuildDstTextureCopyInfoReturnsNulloptForEmptyBounds) {
  TestHWLayer layer(100, 80);

  auto info = layer.BuildDstTextureCopyInfo(skity::Rect{});

  EXPECT_FALSE(info.has_value());
}

TEST(HWTextureCopyInfo, BuildDstTextureCopyInfoClampsAndRoundsBounds) {
  TestHWLayer layer(100, 80);

  auto info = layer.BuildDstTextureCopyInfo(
      skity::Rect::MakeLTRB(-3.4f, 10.2f, 102.8f, 20.9f));

  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->copy_rect, skity::Rect::MakeLTRB(0.f, 10.f, 100.f, 21.f));
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
  TestHWLayer layer(100, 80);
  layer.SetRTOrigin(skity::LayerRTOrigin::kBottomLeft);

  auto uv = layer.BuildDstUVMapping(skity::Rect::MakeLTRB(10.f, 20.f, 30.f, 50.f));

  EXPECT_FLOAT_EQ(uv.x, 1.f / 20.f);
  EXPECT_FLOAT_EQ(uv.y, 1.f / 30.f);
  EXPECT_FLOAT_EQ(uv.z, -10.f / 20.f);
  EXPECT_FLOAT_EQ(uv.w, -30.f / 30.f);
}
