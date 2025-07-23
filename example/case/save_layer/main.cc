// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/save_layer/save_layer_example.hpp"
#include "common/app.hpp"

class SaveLayerExample : public skity::example::WindowClient {
 public:
  SaveLayerExample() = default;
  ~SaveLayerExample() override = default;

 protected:
  void OnDraw(skity::GPUContext* context, skity::Canvas* canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::save_layer::draw_save_layer(canvas, glfwGetTime());
  }
};

int main(int argc, const char** argv) {
  SaveLayerExample example;
  return skity::example::StartExampleApp(argc, argv, example, 1000, 800,
                                         "Save Layer Example");
}
