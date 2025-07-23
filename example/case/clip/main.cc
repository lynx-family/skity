// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/clip/clip_example.hpp"
#include "common/app.hpp"

class ClipExampleCase : public skity::example::WindowClient {
 public:
  ClipExampleCase() = default;

  ~ClipExampleCase() override = default;

  void OnDraw(skity::GPUContext*, skity::Canvas* canvas) override {
    canvas->DrawColor(skity::Color_WHITE);

    skity::example::clip::draw_clip_demo(canvas);

    canvas->Save();

    canvas->Translate(400, 0);

    skity::example::clip::draw_clip_difference(canvas);

    canvas->Restore();
  }
};

int main(int argc, const char** argv) {
  ClipExampleCase example;

  return skity::example::StartExampleApp(argc, argv, example, 800, 800,
                                         "Clip Example");
}
