// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_TEXTURE_HPP
#define SRC_GPU_GPU_TEXTURE_HPP

#include <stdint.h>

#include <functional>
#include <memory>
#include <skity/gpu/texture.hpp>

namespace skity {

enum class GPUTextureFormat {
  kR8Unorm,
  kRGB8Unorm,
  kRGB565Unorm,
  kRGBA8Unorm,
  kBGRA8Unorm,
  kStencil8,
  kDepth24Stencil8,
  kInvalid,
};

constexpr unsigned int GetTextureFormatBytesPerPixel(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return 1;
    case GPUTextureFormat::kRGB565Unorm:
      return 2;
    // @warning: Metal doesn't support 24-bit pixel format
    case GPUTextureFormat::kRGB8Unorm:
    case GPUTextureFormat::kRGBA8Unorm:
      return 4;
    case GPUTextureFormat::kBGRA8Unorm:
      return 4;
    case GPUTextureFormat::kStencil8:
      return 1;
    case GPUTextureFormat::kDepth24Stencil8:
      return 4;
    case GPUTextureFormat::kInvalid:
      return 0;
  }
  return 4;
}

using GPUTextureUsageMask = uint32_t;

enum class GPUTextureUsage : GPUTextureUsageMask {
  kCopySrc = 0x01,
  kCopyDst = 0x02,
  kTextureBinding = 0x04,
  kStorageBinding = 0x08,
  kRenderAttachment = 0x10,
};

enum class GPUTextureStorageMode { kHostVisible, kPrivate, kMemoryless };

struct GPUTextureDescriptor {
  uint32_t width = {};
  uint32_t height = {};
  uint32_t mip_level_count = 1;
  uint32_t sample_count = 1;
  GPUTextureFormat format;
  GPUTextureUsageMask usage;
  GPUTextureStorageMode storage_mode;
};

class GPUTexture {
 public:
  const GPUTextureDescriptor& GetDescriptor() const;

  virtual size_t GetBytes() const = 0;

  void SetRelease(ReleaseCallback release_callback,
                  ReleaseUserData release_user_data);

 protected:
  explicit GPUTexture(const GPUTextureDescriptor& descriptor);

  virtual ~GPUTexture();

 protected:
  GPUTextureDescriptor desc_;
  ReleaseCallback release_callback_ = nullptr;
  ReleaseUserData release_user_data_ = nullptr;
};

using InitializeTextureProc = std::function<std::shared_ptr<GPUTexture>()>;
class GPUTextureProxy {
 public:
  explicit GPUTextureProxy(InitializeTextureProc initialize_proc)
      : initialize_proc_(initialize_proc) {}

  explicit GPUTextureProxy(std::shared_ptr<GPUTexture> texture)
      : texture_(texture) {}

  std::shared_ptr<GPUTexture> GetTexture() {
    if (initialize_proc_ == nullptr && !texture_) {
      return nullptr;
    }

    if (!texture_) {
      texture_ = initialize_proc_();
    }
    return texture_;
  }

 private:
  InitializeTextureProc initialize_proc_ = nullptr;
  std::shared_ptr<GPUTexture> texture_;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_TEXTURE_HPP
