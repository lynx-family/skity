// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/color_space/color_space_example.hpp"
#include "common/app.hpp"

class ColorSpaceExample : public skity::example::WindowClient {
 public:
  ColorSpaceExample() = default;

  ~ColorSpaceExample() override = default;

 protected:
  void OnStart(skity::GPUContext*) override {
    // Renders the same showcase into a Display P3 tagged bitmap and saves
    // it as a PNG with gamut metadata (gAMA/cHRM); ColorSync aware viewers
    // display it as wide gamut.
    skity::example::color_space::export_display_p3_png();
  }

  void OnDraw(skity::GPUContext*, skity::Canvas* canvas) override {
    skity::example::color_space::draw_color_space_example(canvas);
  }
};

int main(int argc, const char** argv) {
  ColorSpaceExample example;

  return skity::example::StartExampleApp(argc, argv, example, 800, 800,
                                         "Color Space Example");
}
