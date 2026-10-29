// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRESENTER_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRESENTER_H

#include <skity/c/skity_base.h>
#include <skity/c/skity_context.h>
#include <skity/c/skity_surface.h>
#include <skity/c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a skity::GPUPresenter. The presenter acquires
 *        one-shot render surfaces and presents them on screen; today only
 *        the Vulkan backend implements it (selected by the create info's
 *        p_next extension).
 */
SKITY_C_DEFINE_HANDLE(skity_presenter);

/**
 * @brief Base presenter creation descriptor. Values mirror
 *        skity::GPUPresenterDescriptor. Set @p p_next to point at a
 *        backend-specific extension (e.g. skity_presenter_create_info_vk)
 *        to select the presenter backend.
 */
typedef struct skity_presenter_create_info {
  skity_structure_type
      s_type;         /**< SKITY_STRUCTURE_TYPE_PRESENTER_CREATE_INFO */
  const void* p_next; /**< → backend-specific extension struct */
  uint32_t width;     /**< physical presentation width in pixels */
  uint32_t height;    /**< physical presentation height in pixels */
} skity_presenter_create_info;

/**
 * @brief Create a presenter and its initial swap resources.
 *
 * @param context  owning GPU context
 * @param info     base creation descriptor; its p_next extension selects
 *                 the backend (skity_presenter_create_info_vk today)
 * @param out_presenter receives the presenter on success
 * @return SKITY_SUCCESS, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result skity_presenter_create(
    skity_context context, const skity_presenter_create_info* info,
    skity_presenter* out_presenter);

/** @brief Destroy the presenter and its swap resources. */
SKITY_C_API void skity_presenter_destroy(skity_presenter presenter);

/** @brief Return the current physical presentation width in pixels. */
SKITY_C_API uint32_t skity_presenter_get_width(skity_presenter presenter);

/** @brief Return the current physical presentation height in pixels. */
SKITY_C_API uint32_t skity_presenter_get_height(skity_presenter presenter);

/**
 * @brief Recreate the presenter swap resources for a new physical size.
 * @return SKITY_SUCCESS on success, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result skity_presenter_resize(skity_presenter presenter,
                                                uint32_t width,
                                                uint32_t height);

/**
 * @brief Acquire the next one-shot render surface. It must be passed to
 *        skity_presenter_present before acquiring another surface.
 */
SKITY_C_API skity_result skity_presenter_acquire_next_surface(
    skity_presenter presenter, uint32_t sample_count, float content_scale,
    skity_surface* out_surface);

/**
 * @brief Present a surface acquired from this presenter. On success or
 *        SKITY_ERROR_NEED_RECREATE the surface handle is consumed and must not
 *        be used or destroyed again.
 */
SKITY_C_API skity_result skity_presenter_present(skity_presenter presenter,
                                                 skity_surface* surface);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRESENTER_H
