// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_CODEC_HPP
#define MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_CODEC_HPP

#include <skity/codec/codec.hpp>

#include "src/codec/data_stream.hpp"
#include "src/codec/wuffs/wuffs_codec_frame.hpp"
#include "src/codec/wuffs/wuffs_module.hpp"

namespace skity {

class WuffsDecoder : public MultiFrameDecoder {
 public:
  WuffsDecoder(WuffsImageDecoder decoder, WuffsBuffer buffer,
               std::shared_ptr<DataStream> stream);
  virtual ~WuffsDecoder() = default;

  int32_t GetWidth() const override;
  int32_t GetHeight() const override;

  int32_t GetFrameCount() const override;

  const CodecFrame* GetFrameInfo(int32_t frame_id) const override;

  std::shared_ptr<Pixmap> DecodeFrame(const CodecFrame* frame) override;

  const std::shared_ptr<DataStream>& GetDataStream() const { return stream_; }

 protected:
  virtual bool OnResetDecoder(const WuffsImageDecoder& decoder) = 0;

 private:
  bool DecodeFrames();

  bool ResetDecoder();

  bool SeekFrame(int32_t frame_index, uint64_t io_position);

  const char* DecodeFrameConfig(wuffs_base__frame_config* frame_config);

  void SetAlphaAndRequiredFrame(CodecFrame* frame);

 private:
  WuffsImageDecoder decoder_;
  WuffsBuffer buffer_;
  std::shared_ptr<DataStream> stream_;
  std::shared_ptr<Data> work_buffer_;

  bool decoder_suspended_;

  wuffs_base__image_config image_config_;

  std::vector<WuffsCodecFrame> frames_;
};

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_WUFFS_WUFFS_CODEC_HPP
