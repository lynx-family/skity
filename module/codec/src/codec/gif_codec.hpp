// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_MODULE_CODEC_GIF_CODEC_HPP
#define SKITY_MODULE_CODEC_GIF_CODEC_HPP

#include <skity/codec/codec.hpp>

namespace skity {

class WuffsDecoder;

class GIFCodec : public Codec {
 public:
  GIFCodec() = default;
  ~GIFCodec() override;

  std::shared_ptr<Pixmap> Decode() override;

  std::shared_ptr<MultiFrameDecoder> DecodeMultiFrame() override;

  std::shared_ptr<Data> Encode(const Pixmap* pixmap) override;

  bool RecognizeFileType(const char* header, size_t size) override;

 private:
  void CreateWuffsDecoderIfNeed();

 private:
  std::shared_ptr<WuffsDecoder> wuffs_decoder_;
};

}  // namespace skity

#endif  // SKITY_MODULE_CODEC_GIF_CODEC_HPP
