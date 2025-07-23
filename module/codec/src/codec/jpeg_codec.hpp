// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_JPEG_CODEC_HPP
#define MODULE_CODEC_SRC_CODEC_JPEG_CODEC_HPP

#ifdef SKITY_HAS_JPEG

#include <skity/codec/codec.hpp>

namespace skity {

class JPEGCodec : public Codec {
 public:
  JPEGCodec() = default;
  ~JPEGCodec() override = default;

  std::shared_ptr<Pixmap> Decode() override;

  std::shared_ptr<Data> Encode(const Pixmap* pixmap) override;

  bool RecognizeFileType(const char* header, size_t size) override;
};

}  // namespace skity

#endif  // SKITY_HAS_JPEG

#endif  // MODULE_CODEC_SRC_CODEC_JPEG_CODEC_HPP
