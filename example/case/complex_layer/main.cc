// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/complex_layer/complex_layer_example.hpp"
#include "common/app.hpp"

class ComplexLayerExampleCase : public skity::example::WindowClient {
 public:
  ComplexLayerExampleCase() = default;

  ~ComplexLayerExampleCase() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::complex_layer::draw_complex_layer(canvas);
  }
};

int main(int argc, const char **argv) {
  ComplexLayerExampleCase example;

  return skity::example::StartExampleApp(argc, argv, example, 1000, 800,
                                         "Complex Layer Example");
}
