// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/webp_codec.hpp"

#include <cstring>

#include "src/codec/webp/webp_decoder.hpp"

namespace skity {

namespace {

std::shared_ptr<WebpDecoder> CreateWebpDecoder(std::shared_ptr<Data> data) {
  if (data == nullptr) {
    return {};
  }

  WebPData webp_data{data->Bytes(), data->Size()};
  WebPDemuxState state{};

  WebPDemuxerPTR demuxer{WebPDemuxPartial(&webp_data, &state)};

  if (state != WEBP_DEMUX_PARSED_HEADER && state != WEBP_DEMUX_DONE) {
    return {};
  }

  return std::make_shared<WebpDecoder>(std::move(demuxer), std::move(data));
}

}  // namespace

WEBPCodec::WEBPCodec() = default;

WEBPCodec::~WEBPCodec() = default;

std::shared_ptr<Pixmap> WEBPCodec::Decode() {
  CreateDecoderIfNeed();

  if (!decoder_) {
    return {};
  }

  auto frame = decoder_->GetFrameInfo(0);

  if (!frame) {
    return {};
  }

  return decoder_->DecodeFrame(frame, nullptr);
}

std::shared_ptr<MultiFrameDecoder> WEBPCodec::DecodeMultiFrame() {
  CreateDecoderIfNeed();

  return decoder_;
}

bool WEBPCodec::RecognizeFileType(const char* header, size_t size) {
  return size >= 14 && std::memcmp(header, "RIFF", 4) == 0 &&
         std::memcmp(header + 8, "WEBPVP", 6) == 0;
}

void WEBPCodec::CreateDecoderIfNeed() {
  if (decoder_ && decoder_->GetData() == data_) {
    return;
  }

  decoder_ = CreateWebpDecoder(data_);
}

}  // namespace skity
