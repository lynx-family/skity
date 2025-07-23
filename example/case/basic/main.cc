// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/basic/example.hpp"
#include "common/app.hpp"

class BasicExampleCase : public skity::example::WindowClient {
 public:
  BasicExampleCase() = default;

  ~BasicExampleCase() override = default;

  void OnDraw(skity::GPUContext*, skity::Canvas* canvas) override {
    canvas->DrawColor(skity::Color_WHITE);

    skity::example::basic::draw_canvas(canvas);
  }
};

int main(int argc, const char** argv) {
  BasicExampleCase basic_case{};

  return skity::example::StartExampleApp(argc, argv, basic_case, 1000, 800,
                                         "Basic Example");
}
