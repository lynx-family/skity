// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/bicubic/bicubic_example.hpp"
#include "common/app.hpp"

class BicubicExample : public skity::example::WindowClient {
 public:
  BicubicExample() = default;
  ~BicubicExample() override = default;

 protected:
  void OnStart(skity::GPUContext* context) override {
    auto pixmap = skity::example::bicubic::LoadBitmap(EXAMPLE_IMAGE_ROOT
                                                      "/mandrill_128.png");
    image_ = skity::example::bicubic::LoadImage(pixmap, context);
  }

  void OnDraw(skity::GPUContext* context, skity::Canvas* canvas) override {
    canvas->Clear(skity::Color_WHITE);
    skity::example::bicubic::DrawBicubic(canvas, image_);
  }

  void OnTerminate() override { image_.reset(); }

 private:
  std::shared_ptr<skity::Image> image_;
};

int main(int argc, const char** argv) {
  BicubicExample example;
  // 2x2 grid of (128*3 + 20) tiles.
  return skity::example::StartExampleApp(argc, argv, example, 850, 850,
                                         "Bicubic Example");
}
