// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/pathop/pathop_example.hpp"
#include "common/app.hpp"

class PathOpExample : public skity::example::WindowClient {
 public:
  PathOpExample() = default;

  ~PathOpExample() override = default;

 protected:
  void OnDraw(skity::GPUContext* context, skity::Canvas* canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::pathop::draw_pathop_example(canvas);
  }
};

int main(int argc, const char** argv) {
  PathOpExample example;
  return skity::example::StartExampleApp(argc, argv, example, 800, 800,
                                         "PathOp Example");
}
