// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_WEBP_WEBP_CODEC_FRAME_HPP
#define MODULE_CODEC_SRC_CODEC_WEBP_WEBP_CODEC_FRAME_HPP

#include <webp/demux.h>

#include <skity/codec/codec.hpp>

namespace skity {

class WebpFrame : public CodecFrame {
 public:
  WebpFrame(int32_t i, const WebPIterator& iter);

  ~WebpFrame() override = default;
};

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_WEBP_WEBP_CODEC_FRAME_HPP
