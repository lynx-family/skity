// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/blend/blend_example.hpp"
#include "common/app.hpp"

class BlendExampleCase : public skity::example::WindowClient {
 public:
  BlendExampleCase() = default;

  ~BlendExampleCase() override = default;

  void OnDraw(skity::GPUContext*, skity::Canvas* canvas) override {
    canvas->DrawColor(skity::Color_WHITE);

    skity::example::blend::draw_blend_case(canvas, 0);
  }
};

int main(int argc, const char** argv) {
  BlendExampleCase case_;

  return skity::example::StartExampleApp(argc, argv, case_, 800, 800,
                                         "Blend Example");
}
