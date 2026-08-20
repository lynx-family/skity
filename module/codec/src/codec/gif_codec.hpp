// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_GIF_CODEC_HPP
#define MODULE_CODEC_SRC_CODEC_GIF_CODEC_HPP

#include <skity/codec/codec.hpp>

namespace skity {

class WuffsDecoder;

class GIFCodec : public Codec {
 public:
  GIFCodec() = default;
  ~GIFCodec() override;

  std::shared_ptr<Pixmap> Decode(const DecodeOptions& options) override;

  std::shared_ptr<MultiFrameDecoder> DecodeMultiFrame() override;

  std::shared_ptr<Data> Encode(const Pixmap* pixmap) override;

  bool RecognizeFileType(const char* header, size_t size) override;

 protected:
  std::shared_ptr<Codec> Fork() override {
    return std::make_shared<GIFCodec>();
  }

 private:
  void CreateWuffsDecoderIfNeed();

  std::shared_ptr<Pixmap> DecodeIntrinsic();

 private:
  std::shared_ptr<WuffsDecoder> wuffs_decoder_;
};

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_GIF_CODEC_HPP
