// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_render_target.hpp>

#include "case/contour_aa/contour_aa_example.hpp"
#include "common/app.hpp"

class ContourAAExample : public skity::example::WindowClient {
 public:
  ContourAAExample() = default;

  ~ContourAAExample() override = default;

 protected:
  void OnStart(skity::GPUContext *context) override {
    if (context == nullptr) {
      return;
    }

    context->SetEnableContourAA(true);

    skity::GPURenderTargetDescriptor desc{};
    desc.width = 1000;
    desc.height = 800;
    desc.sample_count = 1;

    auto render_target = context->CreateRenderTarget(desc);

    if (render_target == nullptr) {
      return;
    }

    auto canvas = render_target->GetCanvas();

    canvas->Clear(skity::Color_WHITE);

    skity::example::contour::aa::draw_contour_aa(canvas);

    result_ = context->MakeSnapshot(std::move(render_target));
  }

  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    if (result_) {
      canvas->DrawImage(result_, 0, 0);
    } else {
      canvas->Clear(skity::Color_RED);

      skity::Paint paint;
      paint.SetColor(skity::Color_BLACK);
      paint.SetTextSize(40.f);

      canvas->DrawSimpleText2("Software Rendering not support contourAA", 10,
                              400, paint);
    }
  }

  void OnTerminate() override { result_.reset(); }

 private:
  std::shared_ptr<skity::Image> result_;
};

int main(int argc, const char **argv) {
  ContourAAExample example;

  return skity::example::StartExampleApp(argc, argv, example, 1000, 800,
                                         "ContourAA Example");
}
