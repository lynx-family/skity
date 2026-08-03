// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/bicubic/bicubic_example.hpp"

#include <skity/codec/codec.hpp>

namespace skity::example::bicubic {

std::shared_ptr<skity::Pixmap> LoadBitmap(const char* path) {
  auto data = skity::Data::MakeFromFileName(path);
  if (!data) {
    return nullptr;
  }
  auto codec = skity::Codec::MakeFromData(data);
  if (!codec) {
    return nullptr;
  }
  codec->SetData(data);
  auto image = codec->Decode();
  if (!image || image->RowBytes() == 0) {
    return nullptr;
  }
  return image;
}

std::shared_ptr<skity::Image> LoadImage(std::shared_ptr<skity::Pixmap> pixmap,
                                        skity::GPUContext* gpu_context) {
  if (!pixmap) {
    return nullptr;
  }
  if (gpu_context != nullptr) {
    auto texture = gpu_context->CreateTexture(
        skity::Texture::FormatFromColorType(pixmap->GetColorType()),
        pixmap->Width(), pixmap->Height(), pixmap->GetAlphaType());
    texture->UploadImage(pixmap);
    return skity::Image::MakeHWImage(texture);
  }
  // software path
  return skity::Image::MakeImage(pixmap);
}

void DrawBicubic(skity::Canvas* canvas,
                 const std::shared_ptr<skity::Image>& image) {
  if (!image) {
    return;
  }
  const float iw = static_cast<float>(image->Width());
  const float ih = static_cast<float>(image->Height());
  // 3x upscale so the sharpness / ringing difference between filters shows up.
  const float scale = 3.f;
  const float dw = iw * scale;
  const float dh = ih * scale;
  const skity::Rect src{0.f, 0.f, iw, ih};
  const float step_x = dw + 20.f;
  const float step_y = dh + 20.f;

  // Four presets laid out in a 2x2 grid:
  //   [ Bilinear         | Catmull-Rom (B=0,   C=0.5) ]
  //   [ Mitchell (1/3)   | B-spline    (B=1,   C=0)   ]
  skity::SamplingOptions bilinear{skity::FilterMode::kLinear,
                                  skity::MipmapMode::kNone};

  skity::SamplingOptions catmull{skity::FilterMode::kLinear,
                                 skity::MipmapMode::kNone};
  catmull.cubic = {0.f, 0.5f};

  skity::SamplingOptions mitchell{skity::FilterMode::kLinear,
                                  skity::MipmapMode::kNone};
  mitchell.cubic = {1.f / 3.f, 1.f / 3.f};

  skity::SamplingOptions bspline{skity::FilterMode::kLinear,
                                 skity::MipmapMode::kNone};
  bspline.cubic = {1.f, 0.f};

  auto draw_at = [&](float x, float y, const skity::SamplingOptions& s) {
    canvas->Save();
    canvas->Translate(x, y);
    canvas->DrawImageRect(image, src, skity::Rect{0.f, 0.f, dw, dh}, s);
    canvas->Restore();
  };

  draw_at(0.f, 0.f, bilinear);       // top-left
  draw_at(step_x, 0.f, catmull);     // top-right
  draw_at(0.f, step_y, mitchell);    // bottom-left
  draw_at(step_x, step_y, bspline);  // bottom-right
}

}  // namespace skity::example::bicubic
