// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/codec/codec.hpp>
#include <skity/effect/color_filter.hpp>
#include <skity/skity.hpp>

namespace skity::example::color_filter {

static std::shared_ptr<skity::Pixmap> g_image;

static bool init_resources(skity::Canvas *) {
  if (g_image) {
    return true;
  }

  const char *path = EXAMPLE_IMAGE_ROOT "/mandrill_128.png";
  auto data = skity::Data::MakeFromFileName(path);

  auto codec = skity::Codec::MakeFromData(data);

  if (!codec) {
    return false;
  }
  codec->SetData(data);
  g_image = codec->Decode();

  if (!g_image || g_image->RowBytes() == 0) {
    g_image = nullptr;
    return false;
  }

  return true;
}

static void draw_matrix_image(skity::Canvas *c, float x, float y,
                              float colorMatrix[20]) {
  skity::Paint paint;
  paint.SetColorFilter(skity::ColorFilters::Matrix(colorMatrix));
  c->DrawImage(skity::Image::MakeImage(g_image),
               skity::Rect::MakeXYWH(x, y, g_image->Width(), g_image->Height()),
               &paint);
}

// same as https://fiddle.skia.org/c/@skpaint_matrix_color_filter
static void draw_color_matrix(skity::Canvas *c, float block_size) {
  float color_matrix[20] = {0, 1, 0, 0, 0,  //
                            0, 0, 1, 0, 0,  //
                            1, 0, 0, 0, 0,  //
                            0, 0, 0, 1, 0};
  draw_matrix_image(c, 0, 0, color_matrix);
  float grayscale[20] = {0.21f, 0.72f, 0.07f, 0.0f, 0.0f,  //
                         0.21f, 0.72f, 0.07f, 0.0f, 0.0f,  //
                         0.21f, 0.72f, 0.07f, 0.0f, 0.0f,  //
                         0.0f,  0.0f,  0.0f,  1.0f, 0.0f};
  draw_matrix_image(c, block_size, 0, grayscale);
  float color_matrix2[20] = {0, 0, 1, 0, 0,  //
                             1, 0, 0, 0, 0,  //
                             0, 1, 0, 0,     //
                             0, 0, 0, 0, 1, 0};
  draw_matrix_image(c, block_size * 2, 0, color_matrix2);
  float inverse[20] = {-1, 0,  0,  1, 0,  //
                       0,  -1, 0,  1, 0,  //
                       0,  0,  -1, 1,     //
                       0,  0,  0,  0, 1, 0};
  draw_matrix_image(c, block_size * 3, 0, inverse);
}

static void draw_color_blend(skity::Canvas *canvas, float block_size) {
  skity::Paint paint;
  paint.SetBlendMode(skity::BlendMode::kSrc);
  paint.SetColor(skity::ColorSetARGB(0x80, 0x00, 0xFF, 0x00));
  canvas->DrawRect(skity::Rect::MakeWH(block_size * 4, block_size), paint);
  paint.Reset();
  paint.SetColorFilter(skity::ColorFilters::Blend(
      skity::ColorSetARGB(0x80, 0xFF, 0x00, 0x00), skity::BlendMode::kSrc));
  canvas->DrawImage(skity::Image::MakeImage(g_image),
                    skity::Rect::MakeWH(g_image->Width(), g_image->Height()),
                    &paint);
  paint.SetColorFilter(skity::ColorFilters::Blend(
      skity::ColorSetARGB(0x80, 0xFF, 0x00, 0x00), skity::BlendMode::kSrcOver));
  canvas->DrawImage(
      skity::Image::MakeImage(g_image),
      skity::Rect::MakeXYWH(block_size, 0, g_image->Width(), g_image->Height()),
      &paint);
  paint.SetAlpha(0x80);
  paint.SetColorFilter(skity::ColorFilters::Blend(
      skity::ColorSetARGB(0x80, 0xFF, 0x00, 0x00), skity::BlendMode::kDst));
  canvas->DrawImage(skity::Image::MakeImage(g_image),
                    skity::Rect::MakeXYWH(block_size * 2, 0, g_image->Width(),
                                          g_image->Height()),
                    &paint);
  paint.SetColorFilter(skity::ColorFilters::Blend(
      skity::ColorSetARGB(0x80, 0xFF, 0x00, 0x00), skity::BlendMode::kDstOver));
  canvas->DrawImage(skity::Image::MakeImage(g_image),
                    skity::Rect::MakeXYWH(block_size * 3, 0, g_image->Width(),
                                          g_image->Height()),
                    &paint);
}

static void draw_compose(skity::Canvas *canvas, float block_size) {
  skity::Paint paint;
  auto srgb_to_linear = skity::ColorFilters::SRGBToLinearGamma();
  auto linear_to_srgb = skity::ColorFilters::LinearToSRGBGamma();
  float grayscale[20] = {0.21f, 0.72f, 0.07f, 0.0f,  0.0f,  0.21f, 0.72f,
                         0.07f, 0.0f,  0.0f,  0.21f, 0.72f, 0.07f, 0.0f,
                         0.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f};
  auto mcf = skity::ColorFilters::Matrix(grayscale);

  auto tcf_mcf = skity::ColorFilters::Compose(srgb_to_linear, mcf);
  paint.SetColorFilter(
      skity::ColorFilters::Compose(linear_to_srgb, srgb_to_linear));
  canvas->DrawImage(
      skity::Image::MakeImage(g_image),
      skity::Rect::MakeXYWH(0, 0, g_image->Width(), g_image->Height()), &paint);

  auto bcf = skity::ColorFilters::Blend(
      skity::ColorSetARGB(0x80, 0xFF, 0x00, 0x00), skity::BlendMode::kSrcOver);

  auto bcf_tcf_mcf = skity::ColorFilters::Compose(bcf, tcf_mcf);
  paint.SetColorFilter(bcf_tcf_mcf);
  canvas->DrawImage(
      skity::Image::MakeImage(g_image),
      skity::Rect::MakeXYWH(block_size, 0, g_image->Width(), g_image->Height()),
      &paint);

  auto l2s = skity::ColorFilters::LinearToSRGBGamma();
  auto l2s_bcf_tcf_mcf = skity::ColorFilters::Compose(l2s, bcf_tcf_mcf);
  paint.SetColorFilter(l2s_bcf_tcf_mcf);
  canvas->DrawImage(skity::Image::MakeImage(g_image),
                    skity::Rect::MakeXYWH(block_size * 2, 0, g_image->Width(),
                                          g_image->Height()),
                    &paint);

  auto s2l = skity::ColorFilters::SRGBToLinearGamma();
  auto s2l_l2s_bcf_tcf_mcf = skity::ColorFilters::Compose(s2l, l2s_bcf_tcf_mcf);
  paint.SetColorFilter(s2l_l2s_bcf_tcf_mcf);
  canvas->DrawImage(skity::Image::MakeImage(g_image),
                    skity::Rect::MakeXYWH(block_size * 3, 0, g_image->Width(),
                                          g_image->Height()),
                    &paint);
}

static void draw_srgb_gamma(skity::Canvas *canvas, float block_size) {
  skity::Paint paint;
  paint.SetColorFilter(skity::ColorFilters::LinearToSRGBGamma());
  canvas->DrawImage(skity::Image::MakeImage(g_image),
                    skity::Rect::MakeWH(g_image->Width(), g_image->Height()),
                    &paint);
  paint.SetColorFilter(skity::ColorFilters::SRGBToLinearGamma());
  canvas->DrawImage(
      skity::Image::MakeImage(g_image),
      skity::Rect::MakeXYWH(block_size, 0, g_image->Width(), g_image->Height()),
      &paint);
}

void draw_filter_example(skity::Canvas *canvas) {
  init_resources(canvas);

  float block_size = 128;

  canvas->Save();
  canvas->Translate(0, block_size);
  draw_color_matrix(canvas, block_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, block_size * 2);
  draw_color_blend(canvas, block_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, block_size * 3);
  draw_compose(canvas, block_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, block_size * 4);
  draw_srgb_gamma(canvas, block_size);
  canvas->Restore();
}

}  // namespace skity::example::color_filter
