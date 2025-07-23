// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/text/text_example.hpp"
#include "common/app.hpp"

class TextExample : public skity::example::WindowClient {
 public:
  TextExample() = default;
  ~TextExample() override = default;

 protected:
  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->DrawColor(skity::Color_WHITE);
    skity::example::text::draw_text_with_emoji(canvas);
  }
};

int main(int argc, const char **argv) {
  TextExample example;
  return skity::example::StartExampleApp(argc, argv, example, 800, 600,
                                         "Text Example");
}
