// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_SKITY_VK_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_SKITY_VK_HPP

// VK aggregator of the header-only RAII layer. Vulkan consumers include
// <skity/skity_vk.hpp> INSTEAD of (or in addition to)
// <skity/skity.hpp>: it pulls in the whole neutral layer plus the
// Vulkan-only wrapper headers below. The VK headers are deliberately kept
// out of skity.hpp so GL / CPU consumers never see <vulkan/vulkan.h>.
//
// Same usage rules as skity.hpp: unified library (SKITY_UNIFIED_LIB) only,
// and never in a TU that also includes the legacy headers under
// include/skity/.

#include <skity/gpu/context_vk.hpp>
#include <skity/gpu/native_window_vk.hpp>
#include <skity/gpu/semaphore_vk.hpp>
#include <skity/gpu/surface_vk.hpp>
#include <skity/gpu/texture_vk.hpp>
#include <skity/skity.hpp>

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_SKITY_VK_HPP
