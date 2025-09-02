// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_CODEC_FRAME_HPP
#define MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_CODEC_FRAME_HPP

#include <skity/codec/codec.hpp>

#include "src/codec/wuffs/wuffs_module.hpp"

namespace skity {

class WuffsCodecFrame : public CodecFrame {
 public:
  explicit WuffsCodecFrame(const wuffs_base__frame_config& config);

  ~WuffsCodecFrame() override = default;

  uint64_t GetIOPos() const { return io_pos_; }

 private:
  uint64_t io_pos_ = 0;
};

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_CODEC_FRAME_HPP
