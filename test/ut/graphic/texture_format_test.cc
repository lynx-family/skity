// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "src/gpu/texture_impl.hpp"

namespace skity {

TEST(TextureFormatMapping, FromGPUTextureFormatMapsSupportedFormats) {
  EXPECT_EQ(FromGPUTextureFormat(GPUTextureFormat::kR8Unorm),
            TextureFormat::kR);
  EXPECT_EQ(FromGPUTextureFormat(GPUTextureFormat::kRGB8Unorm),
            TextureFormat::kRGB);
  EXPECT_EQ(FromGPUTextureFormat(GPUTextureFormat::kRGB565Unorm),
            TextureFormat::kRGB565);
  EXPECT_EQ(FromGPUTextureFormat(GPUTextureFormat::kRGBA8Unorm),
            TextureFormat::kRGBA);
  EXPECT_EQ(FromGPUTextureFormat(GPUTextureFormat::kBGRA8Unorm),
            TextureFormat::kBGRA);
  EXPECT_EQ(FromGPUTextureFormat(GPUTextureFormat::kStencil8),
            TextureFormat::kS);
}

TEST(TextureFormatMapping, ToGPUTextureFormatMapsSupportedFormats) {
  EXPECT_EQ(ToGPUTextureFormat(TextureFormat::kR), GPUTextureFormat::kR8Unorm);
  EXPECT_EQ(ToGPUTextureFormat(TextureFormat::kRGB),
            GPUTextureFormat::kRGB8Unorm);
  EXPECT_EQ(ToGPUTextureFormat(TextureFormat::kRGB565),
            GPUTextureFormat::kRGB565Unorm);
  EXPECT_EQ(ToGPUTextureFormat(TextureFormat::kRGBA),
            GPUTextureFormat::kRGBA8Unorm);
  EXPECT_EQ(ToGPUTextureFormat(TextureFormat::kBGRA),
            GPUTextureFormat::kBGRA8Unorm);
  EXPECT_EQ(ToGPUTextureFormat(TextureFormat::kS), GPUTextureFormat::kStencil8);
}

}  // namespace skity
