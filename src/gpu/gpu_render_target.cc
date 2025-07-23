// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/gpu/gpu_render_target.hpp>

namespace skity {

GPURenderTarget::GPURenderTarget(std::unique_ptr<GPUSurface> surface,
                                 std::shared_ptr<Texture> texture)
    : recorder_(), surface_(std::move(surface)), texture_(std::move(texture)) {
  recorder_.BeginRecording();
}

uint32_t GPURenderTarget::GetWidth() const { return surface_->GetWidth(); }

uint32_t GPURenderTarget::GetHeight() const { return surface_->GetHeight(); }

}  // namespace skity
