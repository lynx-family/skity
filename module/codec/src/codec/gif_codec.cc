// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/gif_codec.hpp"

#include <cstring>
#include <iostream>

#include "src/codec/data_stream.hpp"
#include "src/codec/wuffs/wuffs_codec.hpp"

namespace skity {

namespace {

WuffsImageDecoder create_gif_decoder() {
  auto decoder = wuffs_gif__decoder::alloc_as__wuffs_base__image_decoder();

  auto status = reinterpret_cast<wuffs_gif__decoder*>(decoder.get())
                    ->initialize(sizeof__wuffs_gif__decoder(), WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS);

  if (status.repr != nullptr) {
    std::cerr << "Failed to initialize gif decoder: " << status.message()
              << std::endl;
  }

  return decoder;
}

}  // namespace

class GIFDecoder : public WuffsDecoder {
 public:
  GIFDecoder(WuffsImageDecoder decoder, WuffsBuffer buffer,
             std::shared_ptr<DataStream> stream)
      : WuffsDecoder(std::move(decoder), std::move(buffer), std::move(stream)) {
  }

  ~GIFDecoder() = default;

 protected:
  bool OnResetDecoder(const WuffsImageDecoder& decoder) override {
    if (decoder == nullptr) {
      return false;
    }

    auto status = reinterpret_cast<wuffs_gif__decoder*>(decoder.get())
                      ->initialize(sizeof__wuffs_gif__decoder(), WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS);

    if (status.repr != nullptr) {
      std::cerr << "Failed to initialize gif decoder: " << status.message()
                << std::endl;
      return false;
    }

    return true;
  }
};

GIFCodec::~GIFCodec() = default;

std::shared_ptr<Pixmap> GIFCodec::Decode() {
  // create a wuffs gif decoder and decode the first frame
  CreateWuffsDecoderIfNeed();

  if (wuffs_decoder_->GetFrameCount() == 0) {
    return {};
  }

  auto first_frame = wuffs_decoder_->GetFrameInfo(0);

  if (!first_frame) {
    return {};
  }

  return wuffs_decoder_->DecodeFrame(first_frame, nullptr);
}

std::shared_ptr<MultiFrameDecoder> GIFCodec::DecodeMultiFrame() {
  CreateWuffsDecoderIfNeed();

  return wuffs_decoder_;
}

std::shared_ptr<Data> GIFCodec::Encode(const Pixmap* pixmap) { return {}; }

bool GIFCodec::RecognizeFileType(const char* header, size_t size) {
  constexpr const char* gif_ptr = "GIF8";
  constexpr size_t gif_len = 4;

  return (size >= gif_len) && (std::memcmp(header, gif_ptr, gif_len) == 0);
}

void GIFCodec::CreateWuffsDecoderIfNeed() {
  if (wuffs_decoder_ == nullptr ||
      (wuffs_decoder_ != nullptr &&
       wuffs_decoder_->GetDataStream()->GetData() != data_)) {
    wuffs_decoder_ = std::make_unique<GIFDecoder>(
        wuffs_gif__decoder::alloc_as__wuffs_base__image_decoder(),
        WuffsBuffer{}, std::make_shared<DataStream>(data_));
  }
}

}  // namespace skity
