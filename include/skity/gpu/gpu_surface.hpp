// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_SURFACE_HPP
#define INCLUDE_SKITY_GPU_GPU_SURFACE_HPP

#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/macros.hpp>
#include <skity/render/canvas.hpp>

namespace skity {

class Pixmap;

/**
 * @struct GPUSurfaceDescriptor
 *
 * controls how to create and init a GPUSurface instance
 */
struct GPUSurfaceDescriptor {
  GPUBackendType backend = GPUBackendType::kNone;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t sample_count = 1;
  float content_scale = 1.f;
};

/**
 * @class GPUSurface represent the rendering surface create by GPUContext
 *
 * GPUSurface can target on a specific GPU texture or a framebuffer
 */
class SKITY_API GPUSurface {
 public:
  virtual ~GPUSurface() = default;

  virtual uint32_t GetWidth() const = 0;

  virtual uint32_t GetHeight() const = 0;

  virtual float ContentScale() const = 0;

  /**
   * Lock canvas for current frame. The canvas is owned by this Surface.
   *
   * @note This function can only be called once per frame in most use case.
   *
   * @param clear indicate whether or not clear current contents rendered by
   *              earlier frame.
   *              Note: don't do clear when calling this function may cause more
   *              runtime memory and may hit the performance
   * @return Canvas*  Canvas instance associated with current frame.
   */
  virtual Canvas* LockCanvas(bool clear = true) = 0;

  /**
   * @brief Flush current frame content. This may trigger a gpu surface flush in
   *        some GPU backend.
   *        This function can only be called once perframe. And need to make
   *        sure Canvas::Flush is called before this function.
   *
   */
  virtual void Flush() = 0;

  /**
   * Read rendering result from GPU into CPU memory
   *
   * @param rect the bounds for reading
   * @return raw pixels copied from GPU or nullptr if there is error
   */
  SKITY_EXPERIMENTAL
  virtual std::shared_ptr<Pixmap> ReadPixels(const Rect& rect) = 0;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_SURFACE_HPP
