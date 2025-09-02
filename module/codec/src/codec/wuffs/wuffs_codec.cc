// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/wuffs/wuffs_codec.hpp"

#include <cstring>
#include <limits>
#include <skity/io/pixmap.hpp>

namespace skity {

namespace {

bool decode_image_config(const WuffsImageDecoder& decoder,
                         wuffs_base__image_config* image_config,
                         WuffsBuffer* buffer, DataStream* stream) {
  wuffs_base__status status;

  while (true) {
    status = decoder->decode_image_config(image_config, &buffer->buffer);

    if (status.repr == nullptr) {
      break;
    } else if (status.repr != wuffs_base__suspension__short_read) {
      return false;
    } else if (!buffer->FillBuffer(stream)) {
      return false;
    }
  }

  // we prefer to decode into RGBA format
  uint32_t pixfmt = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL;

  image_config->pixcfg.set(pixfmt, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE,
                           image_config->pixcfg.width(),
                           image_config->pixcfg.height());

  return true;
}

}  // namespace

WuffsDecoder::WuffsDecoder(WuffsImageDecoder decoder, WuffsBuffer buffer,
                           std::shared_ptr<DataStream> stream)
    : decoder_(std::move(decoder)),
      buffer_(std::move(buffer)),
      stream_(std::move(stream)),
      work_buffer_(),
      decoder_suspended_(false),
      image_config_(wuffs_base__null_image_config()),
      frames_() {
  // decode image config

  if (!decode_image_config(decoder_, &image_config_, &buffer_, stream_.get())) {
    return;
  }

  auto width = image_config_.pixcfg.width();
  auto height = image_config_.pixcfg.height();

  if (width == 0 || height == 0 ||
      width > std::numeric_limits<int32_t>::max() ||
      height > std::numeric_limits<int32_t>::max()) {
    return;
  }

  auto workbuff_len = decoder_->workbuf_len().max_incl;

  if (workbuff_len > 0) {
    work_buffer_ =
        Data::MakeFromMalloc(std::malloc(workbuff_len), workbuff_len);

    if (!work_buffer_) {
      return;
    }
  }

  DecodeFrames();
}

int32_t WuffsDecoder::GetWidth() const { return image_config_.pixcfg.width(); }

int32_t WuffsDecoder::GetHeight() const {
  return image_config_.pixcfg.height();
}

int32_t WuffsDecoder::GetFrameCount() const { return frames_.size(); }

const CodecFrame* WuffsDecoder::GetFrameInfo(int32_t frame_id) const {
  if (frame_id < 0 || frame_id >= frames_.size()) {
    return nullptr;
  }

  return &frames_[frame_id];
}

std::shared_ptr<Pixmap> WuffsDecoder::DecodeFrame(const CodecFrame* frame,
                                                  std::shared_ptr<Pixmap>) {
  if (!frame) {
    return {};
  }

  auto width = GetWidth();
  auto height = GetHeight();

  auto wuffs_frame = static_cast<const WuffsCodecFrame*>(frame);

  if (!SeekFrame(wuffs_frame->GetFrameID(), wuffs_frame->GetIOPos())) {
    return {};
  }

  // create pixmap to store decoded frame
  auto pixmap = std::make_shared<Pixmap>(width, height);

  wuffs_base__pixel_config pixel_config;
  pixel_config.set(WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
                   WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width, height);

  wuffs_base__slice_u8 table_slice = wuffs_base__make_slice_u8(
      pixmap->WritableAddr8(0, 0), pixmap->Width() * pixmap->Height() * 4);
  wuffs_base__pixel_buffer pixel_buffer = wuffs_base__null_pixel_buffer();
  auto status = pixel_buffer.set_from_slice(&pixel_config, table_slice);

  if (status.repr != nullptr) {
    return {};
  }

  wuffs_base__pixel_blend pixel_blend = WUFFS_BASE__PIXEL_BLEND__SRC;
  if (wuffs_frame->GetFrameID() != 0 &&
      wuffs_frame->GetRequiredFrame() != CodecFrameInfo::kNoFrameRequired) {
    pixel_blend = WUFFS_BASE__PIXEL_BLEND__SRC_OVER;
  } else {
    // clear pixmap if it is the first frame
    std::memset(pixmap->WritableAddr8(0, 0), 0,
                pixmap->Width() * pixmap->Height() * 4);
  }

  while (true) {
    wuffs_base__slice_u8 a_workbuf;
    if (work_buffer_) {
      a_workbuf = wuffs_base__make_slice_u8(
          const_cast<uint8_t*>(work_buffer_->Bytes()), work_buffer_->Size());
    } else {
      a_workbuf = wuffs_base__make_slice_u8(nullptr, 0);
    }

    status = decoder_->decode_frame(&pixel_buffer, &buffer_.buffer, pixel_blend,
                                    a_workbuf, nullptr);

    if (status.repr == wuffs_base__suspension__short_read &&
        buffer_.FillBuffer(stream_.get())) {
      continue;
    }
    decoder_suspended_ = !status.is_complete();
    break;
  }

  if (status.repr != nullptr) {
    return {};
  }

  return pixmap;
}

bool WuffsDecoder::DecodeFrames() {
  if (!SeekFrame(0, image_config_.first_frame_io_position())) {
    return false;
  }

  for (int32_t i = 0; i < std::numeric_limits<int32_t>::max(); i++) {
    wuffs_base__frame_config frame_config = wuffs_base__null_frame_config();

    auto status = DecodeFrameConfig(&frame_config);

    if (status == nullptr) {
      // No-op
    } else if (status == wuffs_base__note__end_of_data) {
      break;
    } else {
      return false;
    }

    if (static_cast<size_t>(i) < frames_.size()) {
      continue;
    }

    frames_.emplace_back(WuffsCodecFrame(frame_config));

    SetAlphaAndRequiredFrame(&frames_.back());
  }

  if (frames_.empty()) {
    return false;
  }
  frames_.back().SetFullyReceived(true);

  return true;
}

bool WuffsDecoder::ResetDecoder() {
  stream_->Rewind();

  buffer_.buffer.meta = wuffs_base__empty_io_buffer_meta();

  if (!OnResetDecoder(decoder_)) {
    return false;
  }

  if (!decode_image_config(decoder_, &image_config_, &buffer_, stream_.get())) {
    return false;
  }

  decoder_suspended_ = false;

  return true;
}

bool WuffsDecoder::SeekFrame(int32_t frame_index, uint64_t io_position) {
  if (decoder_suspended_) {
    auto res = ResetDecoder();

    if (!res) {
      return false;
    }
  }

  if (frame_index == 0 &&
      io_position != image_config_.first_frame_io_position()) {
    return false;
  }

  if (frame_index < 0 || (!frames_.empty() && frame_index >= frames_.size())) {
    return false;
  }

  if (!buffer_.SeekBuffer(stream_.get(), io_position)) {
    return false;
  }

  auto status = decoder_->restart_frame(frame_index, io_position);

  if (status.repr != nullptr) {
    return false;
  }

  return true;
}

const char* WuffsDecoder::DecodeFrameConfig(
    wuffs_base__frame_config* frame_config) {
  while (true) {
    auto status = decoder_->decode_frame_config(frame_config, &buffer_.buffer);

    if ((status.repr == wuffs_base__suspension__short_read) &&
        (!buffer_.FillBuffer(stream_.get()))) {
      continue;
    }

    decoder_suspended_ = !status.is_complete();

    return status.repr;
  }
}

}  // namespace skity
