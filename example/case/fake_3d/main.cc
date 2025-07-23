// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/fake_3d/fake_3d_example.hpp"
#include "common/app.hpp"

class Fake3DExample : public skity::example::WindowClient {
 public:
  Fake3DExample() = default;

  ~Fake3DExample() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::basic::draw_fake3d(canvas);
  }
};

int main(int argc, const char *argv[]) {
  Fake3DExample example;

  return skity::example::StartExampleApp(argc, argv, example, 800, 800,
                                         "Fake 3D");
}
