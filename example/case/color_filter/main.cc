// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/color_filter/color_filter_example.hpp"
#include "common/app.hpp"

class ColorFilterExample : public skity::example::WindowClient {
 public:
  ColorFilterExample() = default;

  ~ColorFilterExample() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    skity::example::color_filter::draw_filter_example(canvas);
  }
};

int main(int argc, const char **argv) {
  ColorFilterExample example;

  return skity::example::StartExampleApp(argc, argv, example, 800, 800,
                                         "Color Filter Example");
}
