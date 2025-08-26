// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/webp_codec.hpp"

#include <cstring>
#include <iostream>

#include "src/codec/data_stream.hpp"
#include "src/codec/wuffs/wuffs_codec.hpp"

namespace skity {

namespace {

WuffsImageDecoder create_webp_decoder() {
  auto decoder = wuffs_webp__decoder::alloc_as__wuffs_base__image_decoder();

  auto status = reinterpret_cast<wuffs_webp__decoder*>(decoder.get())
                    ->initialize(sizeof__wuffs_webp__decoder(), WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS);

  if (status.repr != nullptr) {
    std::cerr << "Failed to initialize webp decoder: " << status.message()
              << std::endl;
  }

  return decoder;
}

}  // namespace

class WebPDecoder : public WuffsDecoder {
 public:
  WebPDecoder(WuffsImageDecoder decoder, WuffsBuffer buffer,
              std::shared_ptr<DataStream> stream)
      : WuffsDecoder(std::move(decoder), std::move(buffer), std::move(stream)) {
  }

  ~WebPDecoder() = default;

 protected:
  bool OnResetDecoder(const WuffsImageDecoder& decoder) override {
    if (decoder == nullptr) {
      return false;
    }

    auto status = reinterpret_cast<wuffs_webp__decoder*>(decoder.get())
                      ->initialize(sizeof__wuffs_webp__decoder(), WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS);

    if (status.repr != nullptr) {
      std::cerr << "Failed to initialize webp decoder: " << status.message()
                << std::endl;
      return false;
    }

    return true;
  }
};

WEBPCodec::WEBPCodec() = default;

WEBPCodec::~WEBPCodec() = default;

std::shared_ptr<Pixmap> WEBPCodec::Decode() {
  CreateWuffsDecoderIfNeed();

  if (wuffs_decoder_->GetFrameCount() == 0) {
    return {};
  }

  auto first_frame = wuffs_decoder_->GetFrameInfo(0);

  if (!first_frame) {
    return {};
  }

  return wuffs_decoder_->DecodeFrame(first_frame);
}

std::shared_ptr<MultiFrameDecoder> WEBPCodec::DecodeMultiFrame() {
  CreateWuffsDecoderIfNeed();

  return wuffs_decoder_;
}

bool WEBPCodec::RecognizeFileType(const char* header, size_t size) {
  return size >= 14 && std::memcmp(header, "RIFF", 4) == 0 &&
         std::memcmp(header + 8, "WEBPVP", 6) == 0;
}

void WEBPCodec::CreateWuffsDecoderIfNeed() {
  if (wuffs_decoder_ == nullptr ||
      (wuffs_decoder_ != nullptr &&
       wuffs_decoder_->GetDataStream()->GetData() != data_)) {
    wuffs_decoder_ = std::make_unique<WebPDecoder>(
        wuffs_webp__decoder::alloc_as__wuffs_base__image_decoder(),
        WuffsBuffer{}, std::make_shared<DataStream>(data_));
  }
}

}  // namespace skity
