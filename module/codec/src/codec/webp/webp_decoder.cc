// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/webp/webp_decoder.hpp"

#include <cstring>
#include <skity/graphic/bitmap.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>

namespace skity {

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

  WebPDecoderConfig config;

  if (WebPInitDecoderConfig(&config) == 0) {
    return nullptr;
  }

  WebPDecBufferPTR dec_buffer(&config.output);

  WebPIterator webp_frame;
  WebPDIteratorPTR auto_iter(&webp_frame);

  if (!WebPDemuxGetFrame(demuxer_.get(), index + 1, &webp_frame)) {
    return nullptr;
  }

  bool independent =
      index == 0
          ? true
          : (frame->GetRequiredFrame() == CodecFrameInfo::kNoFrameRequired);

  CodecRect screen_rect = {0, 0, frame_width_, frame_height_};
  auto frame_rect = frame->GetRect();

  if (!screen_rect.Contains(frame_rect)) {
    // some thing is wrong
    return nullptr;
  }

  bool frame_is_subset = screen_rect != frame_rect;

  std::shared_ptr<Pixmap> pixmap =
      std::make_shared<Pixmap>(frame_width_, frame_height_,
                               AlphaType::kPremul_AlphaType, ColorType::kRGBA);

  std::memset(pixmap->WritableAddr8(0, 0), 0,
              pixmap->Width() * pixmap->Height() * 4);

  Bitmap temp_canvas_map(pixmap, false);

  auto canvas = Canvas::MakeSoftwareCanvas(&temp_canvas_map);

  bool blend_with_prev_frame =
      !independent && webp_frame.blend_method == WEBP_MUX_BLEND;

  if (blend_with_prev_frame || frame_is_subset) {
    if (prev_pixmap == nullptr) {
      // If needs blend with prev frame, but prev pixmap is null,
      // then just return empty pixmap.
      return {};
    }

    canvas->DrawImage(Image::MakeImage(prev_pixmap), 0, 0);
  }

  std::shared_ptr<Pixmap> tmp_decode_buffer;

  if (blend_with_prev_frame) {
    // needs to decode to a temp buffer first.
    tmp_decode_buffer = std::make_shared<Pixmap>(frame_width_, frame_height_,
                                                 AlphaType::kUnpremul_AlphaType,
                                                 ColorType::kRGBA);

    config.output.colorspace = MODE_RGBA;  // RGBA unpremultiplied alpha
    config.output.is_external_memory = 1;
    config.output.u.RGBA.rgba = tmp_decode_buffer->WritableAddr8(0, 0);
    config.output.u.RGBA.stride = tmp_decode_buffer->RowBytes();
    config.output.u.RGBA.size =
        tmp_decode_buffer->Width() * tmp_decode_buffer->Height() * 4;
  } else {
    config.output.colorspace = MODE_RGBA;  // RGBA unpremultiplied alpha
    config.output.is_external_memory = 1;
    config.output.u.RGBA.rgba =
        pixmap->WritableAddr8(frame_rect.left, frame_rect.top);
    config.output.u.RGBA.stride = pixmap->RowBytes();
    config.output.u.RGBA.size = pixmap->Width() * pixmap->Height() * 4;
  }

  WebPIDecoderPTR idec(WebPIDecode(nullptr, 0, &config));

  if (!idec) {
    return nullptr;
  }

  auto status = WebPIUpdate(idec.get(), webp_frame.fragment.bytes,
                            webp_frame.fragment.size);

  bool success = false;
  int32_t rows_decoded = 0;
  switch (status) {
    case VP8_STATUS_OK:
      rows_decoded = frame_rect.Height();
      success = true;
      break;
    case VP8_STATUS_SUSPENDED: {
      if (!WebPIDecGetRGB(idec.get(), &rows_decoded, nullptr, nullptr,
                          nullptr) ||
          rows_decoded <= 0) {
        success = false;
      }
    } break;
    default:
      success = false;
      break;
  }

  if (!success) {
    return nullptr;
  }

  if (blend_with_prev_frame) {
    Paint paint;

    if (webp_frame.blend_method != WEBP_MUX_BLEND) {
      paint.SetBlendMode(BlendMode::kSrc);
    }

    canvas->DrawImage(Image::MakeImage(tmp_decode_buffer), frame_rect.left,
                      frame_rect.left, SamplingOptions{}, &paint);
  }

  if (pixmap->GetAlphaType() != kUnpremul_AlphaType) {
    pixmap->SetColorInfo(AlphaType::kUnpremul_AlphaType,
                         pixmap->GetColorType());
  }

  return pixmap;
}

}  // namespace skity
