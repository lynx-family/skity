// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/recorder/recorder_example.hpp"
#include "common/app.hpp"

class RecorderExample : public skity::example::WindowClient {
 public:
  RecorderExample() = default;

  ~RecorderExample() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->DrawColor(skity::Color_WHITE);

    skity::example::recorder::draw_with_recorder(canvas);
  }
};

int main(int argc, const char **argv) {
  RecorderExample example;
  return skity::example::StartExampleApp(argc, argv, example, 1000, 800,
                                         "Recorder Example");
}
