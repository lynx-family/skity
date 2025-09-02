// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_WEBP_WEBP_DECODER_HPP
#define MODULE_CODEC_SRC_CODEC_WEBP_WEBP_DECODER_HPP

#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/mux_types.h>

#include <memory>
#include <skity/codec/codec.hpp>
#include <skity/io/data.hpp>

#include "src/codec/webp/webp_codec_frame.hpp"

namespace skity {

class WebPDemuxerPTR
    : public std::unique_ptr<WebPDemuxer, decltype(&WebPDemuxDelete)> {
 public:
  WebPDemuxerPTR(WebPDemuxer* demuxer)
      : std::unique_ptr<WebPDemuxer, decltype(&WebPDemuxDelete)>(
            demuxer, WebPDemuxDelete) {}
};

class WebPDIteratorPTR
    : public std::unique_ptr<WebPIterator,
                             decltype(&WebPDemuxReleaseIterator)> {
 public:
  WebPDIteratorPTR(WebPIterator* iter)
      : std::unique_ptr<WebPIterator, decltype(&WebPDemuxReleaseIterator)>(
            iter, WebPDemuxReleaseIterator) {}
};

class WebPDecBufferPTR
    : public std::unique_ptr<WebPDecBuffer, decltype(&WebPFreeDecBuffer)> {
 public:
  WebPDecBufferPTR(WebPDecBuffer* buffer)
      : std::unique_ptr<WebPDecBuffer, decltype(&WebPFreeDecBuffer)>(
            buffer, WebPFreeDecBuffer) {}
};

class WebPIDecoderPTR
    : public std::unique_ptr<WebPIDecoder, decltype(&WebPIDelete)> {
 public:
  WebPIDecoderPTR(WebPIDecoder* decoder)
      : std::unique_ptr<WebPIDecoder, decltype(&WebPIDelete)>(decoder,
                                                              WebPIDelete) {}
};

class WebpDecoder : public MultiFrameDecoder {
 public:
  WebpDecoder(WebPDemuxerPTR demuxer, std::shared_ptr<Data> data);
  ~WebpDecoder() override = default;

  int32_t GetWidth() const override;

  int32_t GetHeight() const override;

  int32_t GetFrameCount() const override;

  const CodecFrame* GetFrameInfo(int32_t frame_id) const override;

  std::shared_ptr<Pixmap> DecodeFrame(
      const CodecFrame* frame, std::shared_ptr<Pixmap> prev_pixmap) override;

  const std::shared_ptr<Data>& GetData() const { return data_; }

 private:
  WebPDemuxerPTR demuxer_;
  std::shared_ptr<Data> data_;

  int32_t frame_width_;
  int32_t frame_height_;
  int32_t frame_count_;

  std::vector<WebpFrame> frames_ = {};
};

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_WEBP_WEBP_DECODER_HPP
