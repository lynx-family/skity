// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/mask_filter/mask_filter_example.hpp"
#include "common/app.hpp"

class MaskFilterExample : public skity::example::WindowClient {
 public:
  MaskFilterExample() = default;

  ~MaskFilterExample() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::mask_filter::draw_filter(canvas);
  }
};

int main(int argc, const char **argv) {
  MaskFilterExample example;

  return skity::example::StartExampleApp(argc, argv, example, 800, 600,
                                         "Mask Filter Example");
}
