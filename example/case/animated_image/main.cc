// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <chrono>
#include <iostream>
#include <skity/codec/codec.hpp>
#include <skity/io/data.hpp>

#include "common/app.hpp"

class AnimatedImageExample : public skity::example::WindowClient {
 public:
  AnimatedImageExample() = default;
  ~AnimatedImageExample() override = default;

 protected:
  void OnStart(skity::GPUContext* context) override {
    auto data =
        skity::Data::MakeFromFileName(EXAMPLE_IMAGE_ROOT "/color_wheel.gif");

    auto codec = skity::Codec::MakeFromData(data);

    if (!codec) {
      std::cerr << "Failed to make codec from data" << std::endl;
      return;
    }

    codec->SetData(data);

    decoder_ = codec->DecodeMultiFrame();

    if (decoder_ == nullptr) {
      std::cerr << "Failed to decode multi frame from codec" << std::endl;
      return;
    }

    std::cout << "animated image decode success:" << std::endl;
    std::cout << "\t frame count: " << decoder_->GetFrameCount() << std::endl;
    std::cout << "\t width: " << decoder_->GetWidth() << std::endl;
    std::cout << "\t height: " << decoder_->GetHeight() << std::endl;

    last_frame_time_ = std::chrono::system_clock::now();
  }

  void OnDraw(skity::GPUContext* context, skity::Canvas* canvas) override {
    canvas->DrawColor(skity::Color_TRANSPARENT, skity::BlendMode::kSrc);

    if (!decoder_) {
      return;
    }

    auto x = (800 - decoder_->GetWidth()) / 2;
    auto y = (800 - decoder_->GetHeight()) / 2;

    if (current_frame_info_ != nullptr &&
        current_frame_info_->GetDuration() != 0) {
      auto current_time = std::chrono::system_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                          current_time - last_frame_time_)
                          .count();

      if (duration >= current_frame_info_->GetDuration() * 3) {
        current_frame_ = nullptr;

        last_frame_time_ = std::chrono::system_clock::now();

        if (current_frame_info_->IsFullyReceived()) {
          current_frame_info_ = 0;
        } else {
          current_frame_info_ =
              decoder_->GetFrameInfo(current_frame_info_->GetFrameID() + 1);
        }
      }
    }

    if (!current_frame_info_) {
      current_frame_info_ = decoder_->GetFrameInfo(0);
    }

    if (!current_frame_info_) {
      return;
    }

    if (!current_frame_) {
      current_frame_ = decoder_->DecodeFrame(current_frame_info_);
    }

    if (!current_frame_) {
      return;
    }

    auto image = skity::Image::MakeImage(current_frame_);

    canvas->DrawImage(image, x, y);
  }

  void OnTerminate() override {}

 private:
  std::shared_ptr<skity::MultiFrameDecoder> decoder_;
  std::shared_ptr<skity::Pixmap> current_frame_;
  const skity::CodecFrame* current_frame_info_ = nullptr;

  std::chrono::system_clock::time_point last_frame_time_ = {};
};

int main(int argc, const char** argv) {
  AnimatedImageExample example;

  return skity::example::StartExampleApp(argc, argv, example, 800, 800,
                                         "Animated Image Example");
}
