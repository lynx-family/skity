// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/dst_read_strategy.hpp"

#include <gtest/gtest.h>

namespace {

// native_advanced: KHR_blend_equation_advanced exposed.
// coherent: the _coherent sub-extension / advancedBlendCoherentOperations.
// framebuffer_fetch: EXT_shader_framebuffer_fetch.
skity::GPUCaps MakeCaps(bool native_advanced, bool coherent,
                        bool framebuffer_fetch) {
  skity::GPUCaps caps;
  caps.supports_native_advanced_blend = native_advanced;
  caps.supports_native_advanced_blend_coherent = native_advanced && coherent;
  caps.supports_framebuffer_fetch = framebuffer_fetch;
  return caps;
}

}  // namespace

TEST(DstReadStrategy, NonAdvancedModeIsNonRequired) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kSrcOver,
                                          MakeCaps(true, true, false)),
            skity::DstReadStrategy::kNonRequired);
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kPlus,
                                          MakeCaps(true, true, true)),
            skity::DstReadStrategy::kNonRequired);
}

// Tier 1: coherent native beats everything (even framebuffer_fetch).
TEST(DstReadStrategy, CoherentNativeHasHighestPriority) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kMultiply,
                                          MakeCaps(true, true, true)),
            skity::DstReadStrategy::kNativeBlend);
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kMultiply,
                                          MakeCaps(true, true, false)),
            skity::DstReadStrategy::kNativeBlend);
  // A few native-able modes resolve identically.
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kScreen,
                                          MakeCaps(true, true, false)),
            skity::DstReadStrategy::kNativeBlend);
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kLuminosity,
                                          MakeCaps(true, true, false)),
            skity::DstReadStrategy::kNativeBlend);
}

// Tier 2: framebuffer_fetch beats non-coherent native — the key mobile fix.
// A tile-based GPU exposing KHR_blend_equation_advanced without the _coherent
// sub-extension must use framebuffer_fetch, not native + per-draw barrier.
TEST(DstReadStrategy, NonCoherentNativeYieldsToFramebufferFetch) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kMultiply,
                                          MakeCaps(true, false, true)),
            skity::DstReadStrategy::kFramebufferFetch);
}

// Tier 3: non-coherent native is still used when framebuffer_fetch is absent
// (typical desktop GL, where glBlendBarrierKHR is cheap).
TEST(DstReadStrategy, NonCoherentNativeUsedWhenNoFramebufferFetch) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kMultiply,
                                          MakeCaps(true, false, false)),
            skity::DstReadStrategy::kNativeBlend);
}

// Tier 4: texture_copy fallback when nothing better is available.
TEST(DstReadStrategy, AdvancedModeFallsBackToTextureCopy) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kMultiply,
                                          MakeCaps(false, false, false)),
            skity::DstReadStrategy::kTextureCopy);
}

TEST(DstReadStrategy, AdvancedModeFallsBackToFramebufferFetch) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kMultiply,
                                          MakeCaps(false, false, true)),
            skity::DstReadStrategy::kFramebufferFetch);
}

TEST(DstReadStrategy, ModulateNeverUsesNativeBlend) {
  // kModulate has no hardware equivalent (ToNativeBlendOp == nullopt), so it
  // never enters the native tiers even when native advanced blend is supported.
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kModulate,
                                          MakeCaps(true, true, false)),
            skity::DstReadStrategy::kTextureCopy);
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kModulate,
                                          MakeCaps(true, true, true)),
            skity::DstReadStrategy::kFramebufferFetch);
  EXPECT_EQ(skity::ResolveDstReadStrategy(skity::BlendMode::kModulate,
                                          MakeCaps(true, false, true)),
            skity::DstReadStrategy::kFramebufferFetch);
}

TEST(DstReadStrategy, ThreeArgOverloadDoesNotDowngradeCoherentNative) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(
                skity::BlendMode::kMultiply, MakeCaps(true, true, false),
                /*supports_texture_copy_dst_read=*/false),
            skity::DstReadStrategy::kNativeBlend);
}

TEST(DstReadStrategy, ThreeArgOverloadDoesNotDowngradeNonCoherentNative) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(
                skity::BlendMode::kMultiply, MakeCaps(true, false, false),
                /*supports_texture_copy_dst_read=*/false),
            skity::DstReadStrategy::kNativeBlend);
}

TEST(DstReadStrategy, ThreeArgOverloadDoesNotDowngradeFramebufferFetch) {
  // Non-coherent native + fb_fetch resolves to fb_fetch, which the texture_copy
  // gate must not downgrade.
  EXPECT_EQ(skity::ResolveDstReadStrategy(
                skity::BlendMode::kMultiply, MakeCaps(true, false, true),
                /*supports_texture_copy_dst_read=*/false),
            skity::DstReadStrategy::kFramebufferFetch);
}

TEST(DstReadStrategy, ThreeArgOverloadDowngradesTextureCopyWhenUnsupported) {
  EXPECT_EQ(skity::ResolveDstReadStrategy(
                skity::BlendMode::kMultiply, MakeCaps(false, false, false),
                /*supports_texture_copy_dst_read=*/false),
            skity::DstReadStrategy::kNonRequired);
}
