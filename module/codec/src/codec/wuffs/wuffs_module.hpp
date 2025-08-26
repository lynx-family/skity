// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_MODULE_HPP
#define MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_MODULE_HPP

#include <array>
#include <skity/codec/codec.hpp>

#define WUFFS_BASE__HAVE_UNIQUE_PTR  // use std::unique_ptr
#include "wuffs-v0.4.c"

namespace skity {

using WuffsImageDecoder = wuffs_base__image_decoder::unique_ptr;

using WuffsImageConfig = wuffs_base__image_config;

class DataStream;

constexpr size_t kWuffsBufferSize = 4096;

struct WuffsBuffer {
  WuffsBuffer();

  WuffsBuffer(const WuffsBuffer&);
  WuffsBuffer& operator=(const WuffsBuffer&);

  WuffsBuffer(WuffsBuffer&&);
  WuffsBuffer& operator=(WuffsBuffer&&);

  ~WuffsBuffer();

  std::array<uint8_t, kWuffsBufferSize> data = {};
  wuffs_base__io_buffer buffer = {};

  bool FillBuffer(DataStream* stream);

  bool SeekBuffer(DataStream* stream, size_t pos);
};

CodecDisposalMethod WuffsToCodecDisposalMethod(
    wuffs_base__animation_disposal method);

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_MODULE_HPP
