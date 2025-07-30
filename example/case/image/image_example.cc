// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cassert>
#include <skity/codec/codec.hpp>
#include <skity/gpu/gpu_context.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

namespace skity::example::image {

namespace {

std::shared_ptr<skity::Image> create_image(
    std::shared_ptr<skity::Pixmap> bitmap, skity::GPUContext* gpu_context) {
  if (!bitmap) {
    return nullptr;
  }
  std::shared_ptr<skity::Image> image;
  if (gpu_context) {
    auto texture = gpu_context->CreateTexture(
        Texture::FormatFromColorType(bitmap->GetColorType()), bitmap->Width(),
        bitmap->Height(), bitmap->GetAlphaType());
    texture->UploadImage(bitmap);
    image = skity::Image::MakeHWImage(texture);
  } else {
    image = skity::Image::MakeImage(bitmap);
  }
  return image;
}

}  // namespace

std::shared_ptr<skity::Pixmap> load_bitmap(const char* path) {
  auto data = skity::Data::MakeFromFileName(path);

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

void draw_bitmap(skity::Canvas* canvas, skity::GPUContext* gpu_context) {
  canvas->Save();
  canvas->Translate(200, 450);
  Rect rect{0, 0, 400, 100};
  skity::Paint paint;
  paint.SetColor(ColorSetARGB(0xFF, 0x00, 0x00, 0x00));
  canvas->DrawRect(rect, paint);
  size_t width = 80;
  size_t height = 100;
  ColorType color_types[4] = {ColorType::kRGBA, ColorType::kBGRA,
                              ColorType::kRGB565, ColorType::kA8};
  for (size_t i = 0; i < 4; i++) {
    canvas->Translate(i > 0 ? 100 : 0, 0);
    Bitmap bitmap(width, height,
                  color_types[i] == ColorType::kRGB565
                      ? AlphaType::kOpaque_AlphaType
                      : AlphaType::kUnpremul_AlphaType,
                  color_types[i]);
    for (size_t j = 0; j < width; j++) {
      for (size_t k = 0; k < width; k++) {
        bitmap.SetPixel(j, k, ColorSetARGB(0x6F, 0xFF, 0xFF, 0x00));
      }
    }
    uint32_t test_color = bitmap.GetPixel(21, 45);
    if (color_types[i] == ColorType::kRGB565) {
      assert(test_color == ColorSetARGB(0xFF, 0xF8, 0xFC, 0x00));
    }

    auto image = create_image(bitmap.GetPixmap(), gpu_context);
    if (image) {
      canvas->DrawImage(image, 0, 0);
    }
  }

  canvas->Restore();
}

void draw_images(skity::Canvas* canvas, skity::GPUContext* gpu_context) {
  auto image_firefox = create_image(
      load_bitmap(EXAMPLE_IMAGE_ROOT "/firefox_64.png"), gpu_context);
  auto image_mandrill = create_image(
      load_bitmap(EXAMPLE_IMAGE_ROOT "/mandrill_128.png"), gpu_context);
  if (image_firefox) {
    canvas->DrawImage(image_firefox, 0, 0);
  }

  if (image_mandrill) {
    // draw full image
    float translate_x = 64.f;
    float translate_y = 64.f;
    skity::Rect src{0.f, 0.f, static_cast<float>(image_mandrill->Width()),
                    static_cast<float>(image_mandrill->Height())};
    skity::Rect dst{translate_x, translate_y,
                    image_mandrill->Width() + translate_x,
                    image_mandrill->Height() + translate_y};
    canvas->DrawImageRect(image_mandrill, src, dst, skity::SamplingOptions());

    // draw bottom half image
    float offset = image_mandrill->Height() / 2.f;
    skity::Rect src2{0.f, offset, static_cast<float>(image_mandrill->Width()),
                     static_cast<float>(image_mandrill->Height())};
    translate_x += image_mandrill->Width();
    skity::Rect dst2{translate_x, translate_y,
                     image_mandrill->Width() + translate_x,
                     image_mandrill->Height() / 2.f + translate_y};
    canvas->DrawImageRect(image_mandrill, src2, dst2, skity::SamplingOptions());

    canvas->Save();
    canvas->Translate(350, 0);
    auto shader = skity::Shader::MakeShader(image_mandrill);

    shader->SetLocalMatrix(Matrix::Scale(2, 2));
    skity::Paint paint;
    paint.SetShader(shader);
    canvas->DrawRect(skity::Rect::MakeWH(256, 256), paint);
    canvas->Restore();
  }

  std::shared_ptr<skity::Image> img;
  std::shared_ptr<skity::DeferredTextureImage> deffered_image;
  auto image1 = load_bitmap(EXAMPLE_IMAGE_ROOT "/image1.jpg");

  if (gpu_context) {
    deffered_image = skity::Image::MakeDeferredTextureImage(
        skity::TextureFormat::kRGBA, 133, 100,
        skity::AlphaType::kUnknown_AlphaType);
    img = deffered_image;
  } else {
    img = create_image(image1, nullptr);
  }

  auto shader = skity::Shader::MakeShader(img);

  skity::Path path;
  path.MoveTo(133 / 2.0, 0);
  path.LineTo(133, 50);
  path.LineTo(133 / 2.0, 100);
  path.LineTo(0, 50);
  path.Close();

  Paint paint;
  paint.SetShader(shader);

  skity::PictureRecorder recorder;
  recorder.BeginRecording();
  recorder.GetRecordingCanvas()->Save();
  recorder.GetRecordingCanvas()->Translate(0, 300);
  recorder.GetRecordingCanvas()->DrawPath(path, paint);
  recorder.GetRecordingCanvas()->DrawImage(
      img, skity::Rect::MakeXYWH(200, 0, 133, 100));
  recorder.GetRecordingCanvas()->Restore();
  auto display_list = recorder.FinishRecording();

  if (gpu_context) {
    // It should not crash even if the texture does not exist at this time
    display_list->Draw(canvas);
    auto texture =
        gpu_context->CreateTexture(skity::TextureFormat::kRGBA, image1->Width(),
                                   image1->Height(), image1->GetAlphaType());
    texture->UploadImage(image1);
    deffered_image->SetTexture(texture);
    deffered_image->SetAlphaType(image1->GetAlphaType());
  }

  display_list->Draw(canvas);

  draw_bitmap(canvas, gpu_context);
}

}  // namespace skity::example::image
