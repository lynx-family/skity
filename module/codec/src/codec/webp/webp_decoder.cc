// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/webp/webp_decoder.hpp"

#include <cstring>
#include <skity/io/pixmap.hpp>

namespace skity {

namespace {

using WebPAnimDecoderPTR =
    std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)>;

WebPAnimDecoderPTR CreateAnimDecoder(const Data* data) {
  if (data == nullptr) {
    return {nullptr, WebPAnimDecoderDelete};
  }

  WebPAnimDecoderOptions options;
  if (!WebPAnimDecoderOptionsInit(&options)) {
    return {nullptr, WebPAnimDecoderDelete};
  }

  options.color_mode = MODE_RGBA;

  WebPData webp_data{data->Bytes(), data->Size()};
  return {WebPAnimDecoderNew(&webp_data, &options), WebPAnimDecoderDelete};
}

std::shared_ptr<Pixmap> PrepareOutputPixmap(std::shared_ptr<Pixmap> pixmap,
                                            int32_t width, int32_t height) {
  if (!pixmap || pixmap->Width() != width || pixmap->Height() != height ||
      pixmap->GetColorType() != ColorType::kRGBA) {
    pixmap = std::make_shared<Pixmap>(
        width, height, AlphaType::kUnpremul_AlphaType, ColorType::kRGBA);
  } else {
    pixmap->SetColorInfo(AlphaType::kUnpremul_AlphaType, ColorType::kRGBA);
  }

  return pixmap;
}

}  // namespace

WebpDecoder::WebpDecoder(WebPDemuxerPTR demuxer, std::shared_ptr<Data> data)
    : demuxer_(std::move(demuxer)), data_(std::move(data)) {
  frame_width_ = WebPDemuxGetI(demuxer_.get(), WEBP_FF_CANVAS_WIDTH);
  frame_height_ = WebPDemuxGetI(demuxer_.get(), WEBP_FF_CANVAS_HEIGHT);
  frame_count_ = WebPDemuxGetI(demuxer_.get(), WEBP_FF_FRAME_COUNT);

  // query all frame info

  for (int32_t i = 0; i < frame_count_; i++) {
    WebPIterator iter;

    WebPDIteratorPTR auto_iter(&iter);

    if (!WebPDemuxGetFrame(demuxer_.get(), i + 1, &iter)) {
      return;
    }

    if (!iter.complete) {
      return;
    }

    frames_.emplace_back(i, iter);

    SetAlphaAndRequiredFrame(&frames_.back());
  }
}

int32_t WebpDecoder::GetWidth() const { return frame_width_; }

int32_t WebpDecoder::GetHeight() const { return frame_height_; }

int32_t WebpDecoder::GetFrameCount() const { return frame_count_; }

const CodecFrame* WebpDecoder::GetFrameInfo(int32_t frame_id) const {
  if (frame_id < 0 || frame_id >= frame_count_) {
    return nullptr;
  }

  return &frames_[frame_id];
}

std::shared_ptr<Pixmap> WebpDecoder::DecodeFrame(
    const CodecFrame* frame, std::shared_ptr<Pixmap> prev_pixmap) {
  if (!frame) {
    return nullptr;
  }

  auto index = frame->GetFrameID();
  if (index < 0 || index >= frame_count_) {
    return nullptr;
  }

  auto anim_decoder = CreateAnimDecoder(data_.get());
  if (!anim_decoder) {
    return nullptr;
  }

  // WebP animation frame dependencies can skip over adjacent frames once
  // blend/dispose rules are considered. Replaying through libwebp's animator
  // keeps the reconstructed canvas correct for the target frame.
  uint8_t* decoded_frame = nullptr;
  int timestamp = 0;
  for (int32_t i = 0; i <= index; ++i) {
    if (!WebPAnimDecoderGetNext(anim_decoder.get(), &decoded_frame,
                                &timestamp)) {
      return nullptr;
    }
  }

  auto pixmap =
      PrepareOutputPixmap(std::move(prev_pixmap), frame_width_, frame_height_);
  std::memcpy(pixmap->WritableAddr8(0, 0), decoded_frame,
              pixmap->RowBytes() * pixmap->Height());

  return pixmap;
}

}  // namespace skity
