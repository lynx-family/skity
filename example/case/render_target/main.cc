// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/render_target/render_target_example.hpp"
#include "common/app.hpp"

class RenderTargetExample : public skity::example::WindowClient {
 public:
  RenderTargetExample() = default;
  ~RenderTargetExample() override = default;

 protected:
  void OnDraw(skity::GPUContext* context, skity::Canvas* canvas) override {
    if (GetWindow()->GetBackend() ==
        skity::example::Window::Backend::kSoftware) {
      canvas->Clear(skity::Color_RED);

      skity::Paint paint;
      paint.SetColor(skity::Color_BLACK);
      paint.SetTextSize(30.f);
      canvas->DrawSimpleText2("Software Rendering not support RenderTarget", 10,
                              400, paint);
    } else {
      canvas->Clear(skity::Color_WHITE);

      skity::example::render_target::draw_render_target(canvas, context);
    }
  }
};

int main(int argc, const char** argv) {
  RenderTargetExample example;
  return skity::example::StartExampleApp(argc, argv, example, 800, 600,
                                         "Render Target Example");
}
