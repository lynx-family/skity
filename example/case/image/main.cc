// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/image/image_example.hpp"
#include "common/app.hpp"

namespace skity::example::image {

std::shared_ptr<skity::Image> MakeImageGL(
    const std::shared_ptr<skity::Pixmap> &pixmap,
    skity::GPUContext *gpu_context);

std::shared_ptr<skity::Image> MakeImageMTL(
    const std::shared_ptr<skity::Pixmap> &pixmap,
    skity::GPUContext *gpu_context);

}  // namespace skity::example::image

class ImageExample : public skity::example::WindowClient {
 public:
  ImageExample() = default;

  ~ImageExample() override = default;

 protected:
  void OnStart(skity::GPUContext *context) override {
    auto backend = GetWindow()->GetBackend();

    auto pixmap =
        skity::example::image::load_bitmap(EXAMPLE_IMAGE_ROOT "/image4.jpg");

    if (backend == skity::example::Window::Backend::kOpenGL) {
      image_ = skity::example::image::MakeImageGL(pixmap, context);
    } else if (backend == skity::example::Window::Backend::kMetal) {
      image_ = skity::example::image::MakeImageMTL(pixmap, context);
    } else if (backend == skity::example::Window::Backend::kSoftware) {
      image_ = skity::Image::MakeImage(pixmap);
    }
  }

  void OnDraw(skity::GPUContext *context, skity::Canvas *canvas) override {
    canvas->Clear(skity::Color_WHITE);

    skity::example::image::draw_images(canvas, context);

    if (image_) {
      canvas->DrawImage(image_, skity::Rect::MakeXYWH(0, 450, image_->Width(),
                                                      image_->Height()));
    }
  }

  void OnTerminate() override { image_.reset(); }

 private:
  std::shared_ptr<skity::Image> image_;
};

int main(int argc, const char **argv) {
  ImageExample example;
  return skity::example::StartExampleApp(argc, argv, example, 800, 600,
                                         "Image Example");
}
