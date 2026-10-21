// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_H

/*
 * Aggregate header for the Skity C API. Include this for the full surface.
 * Vulkan context creation (skity_context_create_vk) is in skity_context_vk.h,
 * which is intentionally separate so that non-Vulkan consumers are not forced
 * to pull in <vulkan/vulkan.h>. The Metal headers (skity_context_mtl.h /
 * skity_surface_mtl.h / skity_texture_mtl.h) are likewise separate: they are
 * Objective-C-only (skity_mtl_types.h), so Objective-C(++) consumers include
 * them directly.
 */

#include <skity/c/skity_base.h>
#include <skity/c/skity_bitmap.h>
#include <skity/c/skity_camera.h>
#include <skity/c/skity_canvas.h>
#include <skity/c/skity_color_filter.h>
#include <skity/c/skity_context.h>
#include <skity/c/skity_data.h>
#include <skity/c/skity_font.h>
#include <skity/c/skity_glyph.h>
#include <skity/c/skity_image.h>
#include <skity/c/skity_image_filter.h>
#include <skity/c/skity_mask_filter.h>
#include <skity/c/skity_paint.h>
#include <skity/c/skity_path.h>
#include <skity/c/skity_path_effect.h>
#include <skity/c/skity_path_measure.h>
#include <skity/c/skity_path_op.h>
#include <skity/c/skity_precompile.h>
#include <skity/c/skity_quaternion.h>
#include <skity/c/skity_recorder.h>
#include <skity/c/skity_semaphore.h>
#include <skity/c/skity_shader.h>
#include <skity/c/skity_stroke.h>
#include <skity/c/skity_surface.h>
#include <skity/c/skity_text.h>
#include <skity/c/skity_texture.h>
#include <skity/c/skity_types.h>

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_H
