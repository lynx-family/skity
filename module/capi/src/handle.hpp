// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_SRC_HANDLE_HPP
#define MODULE_CAPI_SRC_HANDLE_HPP

#include <cstdint>
#include <memory>
#include <new>

/*
 * Object type tag + handle header. These are implementation details of the
 * wrapper and are intentionally NOT exposed in the public C headers — handles
 * are fully opaque to consumers. The header is the first member of every
 * wrapper struct so that resolve() can validate the type tag before use.
 */
typedef enum {
  SKITY_OBJECT_TYPE_UNKNOWN = 0,
  SKITY_OBJECT_TYPE_CONTEXT = 1,
  SKITY_OBJECT_TYPE_SURFACE,
  SKITY_OBJECT_TYPE_CANVAS,
  SKITY_OBJECT_TYPE_PAINT,
  SKITY_OBJECT_TYPE_PATH,
  SKITY_OBJECT_TYPE_SHADER,
  SKITY_OBJECT_TYPE_COLOR_FILTER,
  SKITY_OBJECT_TYPE_IMAGE_FILTER,
  SKITY_OBJECT_TYPE_MASK_FILTER,
  SKITY_OBJECT_TYPE_PATH_EFFECT,
  SKITY_OBJECT_TYPE_TYPEFACE,
  SKITY_OBJECT_TYPE_FONT_MANAGER,
  SKITY_OBJECT_TYPE_PICTURE_RECORDER,
  SKITY_OBJECT_TYPE_DISPLAY_LIST,
  SKITY_OBJECT_TYPE_IMAGE,
  SKITY_OBJECT_TYPE_TEXT_BLOB,
  SKITY_OBJECT_TYPE_TEXTURE,
  SKITY_OBJECT_TYPE_FONT_STYLE_SET,
  SKITY_OBJECT_TYPE_PRECOMPILE_CONTEXT,
  SKITY_OBJECT_TYPE_FONT,
  SKITY_OBJECT_TYPE_BITMAP,
  SKITY_OBJECT_TYPE_PIXMAP,
  SKITY_OBJECT_TYPE_PATH_MEASURE,
  SKITY_OBJECT_TYPE_CAMERA,
  SKITY_OBJECT_TYPE_DATA,
  SKITY_OBJECT_TYPE_TYPEFACE_DELEGATE,
} skity_object_type;

/* When set, the wrapper owns the underlying object and releases it on destroy.
 * When clear (e.g. the Canvas from skity_surface_lock_canvas, owned by the
 * surface), destroy only reclaims the wrapper struct. */
#define SKITY_HANDLE_OWNING 0x1u

typedef struct skity_object_header {
  uint32_t type;  /* skity_object_type */
  uint32_t flags; /* SKITY_HANDLE_OWNING | reserved */
} skity_object_header;

/*
 * Wrapper-internal struct definitions. Each is `header + impl` where impl is a
 * type-erased shared_ptr:
 *   - owning handles store a shared_ptr with the default deleter (or the
 *     factory's original shared_ptr), so destroying the wrapper releases the
 *     object;
 *   - non-owning handles (e.g. Canvas returned by lock_canvas) store the raw
 *     pointer with an empty deleter, so destroying the wrapper is a no-op on
 *     the underlying object.
 *
 * These structs live at global scope so they match the C-side typedefs
 * produced by SKITY_C_DEFINE_HANDLE.
 */
struct skity_context_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_surface_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_canvas_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_paint_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_path_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_shader_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_color_filter_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_image_filter_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_mask_filter_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_path_effect_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_typeface_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_font_manager_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_picture_recorder_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_display_list_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_image_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_text_blob_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_texture_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_font_style_set_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_precompile_context_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_font_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_bitmap_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_pixmap_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_path_measure_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_camera_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_data_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};
struct skity_typeface_delegate_s {
  skity_object_header header;
  std::shared_ptr<void> impl;
};

namespace skity {
namespace capi {

/*
 * Allocate a wrapper struct with its header filled in. Uses nothrow new so a
 * failed allocation returns nullptr (the wrapper is built with -fno-exceptions,
 * matching the core skity library).
 */
template <typename Wrapper>
inline Wrapper* alloc_handle(skity_object_type type, uint32_t flags,
                             std::shared_ptr<void> impl) {
  Wrapper* w = new (std::nothrow) Wrapper{};
  if (w == nullptr) return nullptr;
  w->header.type = type;
  w->header.flags = flags;
  w->impl = std::move(impl);
  return w;
}

/*
 * Validate the handle's type tag and return it as its concrete wrapper struct,
 * or nullptr if the handle is null / wrong type. The first-member header
 * invariant lets every wrapper be read as skity_object_header*.
 */
template <typename Wrapper>
inline Wrapper* resolve(void* handle, skity_object_type expected) {
  if (handle == nullptr) return nullptr;
  auto* hdr = static_cast<skity_object_header*>(handle);
  return hdr->type == expected ? static_cast<Wrapper*>(handle) : nullptr;
}

/*
 * Reclaim a wrapper. The shared_ptr impl field's deleter decides whether the
 * underlying object is released (owning) or left untouched (non-owning), so
 * this is uniformly safe regardless of the owning flag.
 */
template <typename Wrapper>
inline void destroy_handle(void* handle, skity_object_type expected) {
  Wrapper* w = resolve<Wrapper>(handle, expected);
  if (w == nullptr) return;
  delete w;
}

/*
 * Resolve a handle and return its impl re-typed to T. Returns an empty
 * shared_ptr on null / wrong type.
 */
template <typename Wrapper, typename T>
inline std::shared_ptr<T> get_impl(void* handle, skity_object_type expected) {
  Wrapper* w = resolve<Wrapper>(handle, expected);
  if (w == nullptr) return nullptr;
  return std::static_pointer_cast<T>(w->impl);
}

}  // namespace capi
}  // namespace skity

#endif  // MODULE_CAPI_SRC_HANDLE_HPP
