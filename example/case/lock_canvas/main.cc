// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <functional>

#include "common/app.hpp"

namespace skity::example::lock_canvas {

#ifdef SKITY_EXAMPLE_GL_BACKEND

std::shared_ptr<skity::Image> DrawOffscreenGL(
    skity::GPUContext *context, int width, int height,
    std::function<void(skity::GPUSurface *surface)> &&func);

#endif

#ifdef SKITY_EXAMPLE_MTL_BACKEND

std::shared_ptr<skity::Image> DrawOffscreenMTL(
    skity::GPUContext *context, int width, int height,
    std::function<void(skity::GPUSurface *surface)> &&func);

#endif

}  // namespace skity::example::lock_canvas

class LockCanvasExample : public skity::example::WindowClient {
 public:
  LockCanvasExample() = default;

  ~LockCanvasExample() override = default;

 protected:
  void OnStart(skity::GPUContext *context) override {
    auto func = [](skity::GPUSurface *surface) {
      auto offscreen_canvas = surface->LockCanvas(true);
      offscreen_canvas->DrawColor(0xff0000ff);
      offscreen_canvas->Flush();
      surface->Flush();

      offscreen_canvas = surface->LockCanvas(false);
      auto rect = skity::Rect::MakeLTRB(0, 0, 400, 300);
      skity::Paint paint;
      paint.SetColor(0xFF00FF00);
      offscreen_canvas->DrawRect(rect, paint);
      offscreen_canvas->Flush();
      surface->Flush();

      offscreen_canvas = surface->LockCanvas(false);
      rect = skity::Rect::MakeLTRB(0, 0, 200, 150);
      paint.SetBlendMode(skity::BlendMode::kClear);
      offscreen_canvas->DrawRect(rect, paint);
      offscreen_canvas->Flush();
      surface->Flush();
    };

    auto backend = GetWindow()->GetBackend();

    if (backend == skity::example::Window::Backend::kOpenGL) {
      result_ =
          skity::example::lock_canvas::DrawOffscreenGL(context, 800, 600, func);
    } else if (backend == skity::example::Window::Backend::kMetal) {
      result_ = skity::example::lock_canvas::DrawOffscreenMTL(context, 800, 600,
                                                              func);
    }
  }

  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    if (result_) {
      if (GetWindow()->GetBackend() ==
          skity::example::Window::Backend::kOpenGL) {
        canvas->Save();
        canvas->SetMatrix(skity::Matrix::Translate(0, result_->Height()) *
                          skity::Matrix::Scale(1, -1));
      }

      canvas->DrawImage(result_, 0, 0);

      if (GetWindow()->GetBackend() ==
          skity::example::Window::Backend::kOpenGL) {
        canvas->Restore();
      }

    } else {
      canvas->Clear(skity::Color_RED);

      skity::Paint paint;
      paint.SetColor(skity::Color_BLACK);
      paint.SetTextSize(30.f);
      canvas->DrawSimpleText2("Software Rendering not support lock canvas", 10,
                              400, paint);
    }
  }

  void OnTerminate() override { result_.reset(); }

 private:
  std::shared_ptr<skity::Image> result_;
};

int main(int argc, const char **argv) {
  LockCanvasExample example;

  return skity::example::StartExampleApp(argc, argv, example, 800, 800,
                                         "Lock Canvas Example");
}
