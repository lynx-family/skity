// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_RENDER_TARGET_HPP
#define INCLUDE_SKITY_GPU_GPU_RENDER_TARGET_HPP

#include <skity/gpu/gpu_surface.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/graphic/image.hpp>
#include <skity/macros.hpp>
#include <skity/recorder/picture_recorder.hpp>

namespace skity {

/**
 * @brief Contains the information of GPURenderTarget.
 *
 */
struct GPURenderTargetDescriptor {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t sample_count = 1;
};

/**
 * @brief GPURenderTarget is a wrapper of GPUSurface and the associate Texture.
 *
 * @note GPURenderTarget can not be reused. And the format of the texture is
 *       decided by the GPUContext when it is created.
 */
class SKITY_API GPURenderTarget {
  friend class GPUContextImpl;

 public:
  GPURenderTarget(std::unique_ptr<GPUSurface> surface,
                  std::shared_ptr<Texture> texture);

  virtual ~GPURenderTarget() = default;

  uint32_t GetWidth() const;

  uint32_t GetHeight() const;

  Canvas* GetCanvas() { return recorder_.GetRecordingCanvas(); }

 private:
  PictureRecorder recorder_;
  std::unique_ptr<GPUSurface> surface_;
  std::shared_ptr<Texture> texture_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_RENDER_TARGET_HPP
