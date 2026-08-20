// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/webp_codec.hpp"

#include <cstring>

#include "src/codec/codec_priv.hpp"
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

std::shared_ptr<Pixmap> WEBPCodec::Decode(const DecodeOptions& options) {
  CreateDecoderIfNeed();

  if (!decoder_) {
    return {};
  }

  auto frame = decoder_->GetFrameInfo(0);

  if (!frame) {
    return {};
  }

  int32_t target_width = 0;
  int32_t target_height = 0;
  if (decoder_->GetFrameCount() == 1 &&
      codec_priv::ResolveTargetSize(decoder_->GetWidth(), decoder_->GetHeight(),
                                    options, &target_width, &target_height)) {
    // Single-frame WebP: let libwebp's rescaler hit the exact target size.
    // On failure fall through to the anim-decoder path below, which also
    // handles single-frame files.
    auto scaled = decoder_->DecodeFirstFrameScaled(target_width, target_height);
    if (scaled) {
      return scaled;
    }
  }

  // Animated WebP keeps the canvas path at intrinsic size in v1 (the anim
  // decoder has no scaling); resample afterwards like the other fallback
  // formats.
  return codec_priv::ResamplePixmapToTarget(
      decoder_->DecodeFrame(frame, nullptr), options);
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
