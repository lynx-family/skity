// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_texture.hpp"

namespace skity {

GPUTexture::GPUTexture(const GPUTextureDescriptor& desc) : desc_(desc) {}

void GPUTexture::SetRelease(ReleaseCallback release_callback,
                            ReleaseUserData release_user_data) {
  release_callback_ = release_callback;
  release_user_data_ = release_user_data;
}

GPUTexture::~GPUTexture() {
  if (release_callback_) {
    release_callback_(release_user_data_);
  }
}

const GPUTextureDescriptor& GPUTexture::GetDescriptor() const { return desc_; }

}  // namespace skity
