// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Compile + argument-validation smoke gate for the Vulkan-only header-only
// wrappers (skity_vk.hpp aggregate). The cases below need no Vulkan device:
// they assert the failure paths that are deterministic without one. The
// full create / present flow is exercised by the golden Vulkan tests; this
// TU belongs to the unified-only skity_wrapper_unit_test binary.

#include <vulkan/vulkan.h>

#include <skity/skity_vk.hpp>

#include "gtest/gtest.h"

namespace {

using skity::Context;
using skity::CreateVkContext;
using skity::CreateVkSurface;
using skity::NativeWindowVk;
using skity::Surface;
using skity::Texture;
using skity::WrapVkTexture;

}  // namespace

TEST(WrapperVkHpp, ContextFactoriesValidateArguments) {
  // A null proc-address loader is rejected by the C layer (null result).
  EXPECT_EQ(CreateVkContext(nullptr), nullptr);

  // A supplied logical device without its proc-address loader is rejected.
  skity_context_create_info_vk info = {};
  info.logical_device = reinterpret_cast<VkDevice>(0x1);
  EXPECT_EQ(CreateVkContext(info), nullptr);
}

TEST(WrapperVkHpp, ChainHelpersRejectNullContext) {
  Context context;  // empty (null) context

  skity_surface_create_info base = {};
  skity_surface_create_info_vk ext = {};
  EXPECT_EQ(CreateVkSurface(context, base, ext), nullptr);

  skity_backend_texture_info tex_base = {};
  skity_backend_texture_info_vk tex_ext = {};
  EXPECT_EQ(WrapVkTexture(context, tex_base, tex_ext), nullptr);
}

TEST(WrapperVkHpp, NativeWindowLifecycleValidation) {
  NativeWindowVk window;  // empty (never created)
  EXPECT_EQ(window.GetWidth(), 0u);
  EXPECT_EQ(window.GetHeight(), 0u);
  EXPECT_EQ(window.Resize(100, 100), SKITY_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(window);

  std::unique_ptr<Surface> surface;
  EXPECT_EQ(window.AcquireNextSurface(1, 1.f, &surface),
            SKITY_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(surface);

  // Present requires a non-empty surface; the empty wrapper is rejected
  // without touching the (null) window handle.
  EXPECT_EQ(window.Present(std::move(surface)), SKITY_ERROR_INVALID_HANDLE);
  EXPECT_EQ(window.Present(nullptr), SKITY_ERROR_INVALID_HANDLE);

  // Creation with a null context fails cleanly.
  Context null_context;
  skity_native_window_create_info_vk info = {};
  EXPECT_FALSE(NativeWindowVk::Create(null_context, info));

  // Adopting a raw NULL handle is equally safe.
  EXPECT_FALSE(NativeWindowVk::Adopt(nullptr));
}
