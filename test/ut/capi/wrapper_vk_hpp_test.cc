// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Compile + argument-validation smoke gate for the Vulkan-only header-only
// wrappers (skity_vk.hpp aggregate). The cases below need no Vulkan device:
// they assert the failure paths that are deterministic without one. The
// full create / present flow is exercised by the golden Vulkan tests.

#include <vulkan/vulkan.h>

#include <skity_hpp/skity_vk.hpp>

#include "gtest/gtest.h"

namespace {

using skity::raii::Context;
using skity::raii::CreateVkContext;
using skity::raii::CreateVkSurface;
using skity::raii::NativeWindowVk;
using skity::raii::Surface;
using skity::raii::Texture;
using skity::raii::WrapVkTexture;

}  // namespace

TEST(WrapperVkHpp, ContextFactoriesValidateArguments) {
  Context context;
  EXPECT_EQ(CreateVkContext(nullptr, &context), SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_FALSE(context);

  // A supplied logical device without its proc-address loader is rejected.
  skity_context_create_info_vk info = {};
  info.logical_device = reinterpret_cast<VkDevice>(0x1);
  EXPECT_EQ(CreateVkContext(info, &context), SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_FALSE(context);
}

TEST(WrapperVkHpp, ChainHelpersRejectNullContext) {
  Context context;  // empty (null) context

  skity_surface_create_info base = {};
  skity_surface_create_info_vk ext = {};
  Surface surface;
  EXPECT_EQ(CreateVkSurface(context, base, ext, &surface),
            SKITY_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(surface);

  skity_backend_texture_info tex_base = {};
  skity_backend_texture_info_vk tex_ext = {};
  Texture texture = WrapVkTexture(context, tex_base, tex_ext);
  EXPECT_FALSE(texture);
  // Null-safe accessors on the empty wrapper.
  EXPECT_EQ(texture.GetWidth(), 0u);
  EXPECT_EQ(texture.GetHeight(), 0u);
}

TEST(WrapperVkHpp, NativeWindowLifecycleValidation) {
  NativeWindowVk window;  // empty (never created)
  EXPECT_EQ(window.GetWidth(), 0u);
  EXPECT_EQ(window.GetHeight(), 0u);
  EXPECT_EQ(window.Resize(100, 100), SKITY_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(window);

  Surface surface;
  EXPECT_EQ(window.AcquireNextSurface(1, 1.f, &surface),
            SKITY_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(surface);

  // Present requires a non-empty surface; the empty wrapper is rejected
  // without touching the (null) window handle.
  EXPECT_EQ(window.Present(&surface), SKITY_ERROR_INVALID_HANDLE);
  EXPECT_EQ(window.Present(nullptr), SKITY_ERROR_INVALID_HANDLE);

  // Creation with a null context fails cleanly.
  Context null_context;
  skity_native_window_create_info_vk info = {};
  EXPECT_FALSE(NativeWindowVk::Create(null_context, info));

  // Adopting a raw NULL handle is equally safe.
  EXPECT_FALSE(NativeWindowVk::Adopt(nullptr));
}
