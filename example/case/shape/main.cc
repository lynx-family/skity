// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/shape/shape_example.hpp"
#include "common/app.hpp"

class ShapeExample : public skity::example::WindowClient {
 public:
  ShapeExample() = default;

  ~ShapeExample() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::shape::draw_shapes(canvas);
  }
};

int main(int argc, const char **argv) {
  ShapeExample example;
  return skity::example::StartExampleApp(argc, argv, example, 800, 600,
                                         "Shape Example");
}
