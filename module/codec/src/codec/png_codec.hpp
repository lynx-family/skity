// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_PNG_CODEC_HPP
#define MODULE_CODEC_SRC_CODEC_PNG_CODEC_HPP

#include <png.h>

#include <skity/codec/codec.hpp>

namespace skity {

class PNGCodec : public Codec {
 public:
  PNGCodec();
  ~PNGCodec() override;
  std::shared_ptr<Pixmap> Decode() override;
  std::shared_ptr<Data> Encode(const Pixmap* pixmap) override;
  bool RecognizeFileType(const char* header, size_t size) override;

 private:
  png_image image_ = {};
  std::shared_ptr<Pixmap> pixmap_ = {};
};

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_PNG_CODEC_HPP
