// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/gradient/gradient_example.hpp"
#include "common/app.hpp"

class GradientExample : public skity::example::WindowClient {
 public:
  GradientExample() = default;

  ~GradientExample() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::gradient::draw_gradient(canvas);
  }
};

int main(int argc, const char *argv[]) {
  GradientExample example;
  return skity::example::StartExampleApp(argc, argv, example, 1000, 800,
                                         "Gradient Example");
}
