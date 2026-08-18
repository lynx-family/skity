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

std::shared_ptr<Pixmap> WebpDecoder::DecodeFirstFrameScaled(
    int32_t target_width, int32_t target_height) {
  if (frame_count_ != 1 || target_width <= 0 || target_height <= 0) {
    return nullptr;
  }

  WebPIterator iter;
  WebPDIteratorPTR auto_iter(&iter);

  if (!WebPDemuxGetFrame(demuxer_.get(), 1, &iter) || !iter.complete) {
    return nullptr;
  }

  // libwebp's rescaler decodes straight to the requested size — any ratio,
  // no second pass. MODE_RGBA output is unpremultiplied, matching the
  // module's canonical pixmap.
  WebPDecoderConfig config;
  if (!WebPInitDecoderConfig(&config)) {
    return nullptr;
  }

  config.output.colorspace = MODE_RGBA;
  config.options.use_scaling = 1;
  config.options.scaled_width = target_width;
  config.options.scaled_height = target_height;

  if (WebPDecode(iter.fragment.bytes, iter.fragment.size, &config) !=
      VP8_STATUS_OK) {
    WebPFreeDecBuffer(&config.output);
    return nullptr;
  }

  auto pixmap = std::make_shared<Pixmap>(
      config.output.width, config.output.height, AlphaType::kUnpremul_AlphaType,
      ColorType::kRGBA);

  // Stride is width * 4 for an internal RGBA buffer, but copy row by row to
  // stay independent of that assumption.
  const uint8_t* rgba = config.output.u.RGBA.rgba;
  int stride = config.output.u.RGBA.stride;
  for (int i = 0; i < config.output.height; i++) {
    std::memcpy(pixmap->WritableAddr8(0, i),
                rgba + static_cast<size_t>(i) * stride,
                static_cast<size_t>(config.output.width) * 4);
  }

  WebPFreeDecBuffer(&config.output);

  return pixmap;
}

}  // namespace skity
