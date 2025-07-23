// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/image_filter/image_filter_example.hpp"

#include <skity/codec/codec.hpp>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/skity.hpp>

#include "common/app_utils.hpp"

namespace skity::example::image_filter {

enum {
  kExampleWidth = 800,
};

static bool g_debug = false;
static std::shared_ptr<skity::Pixmap> g_image_firefox;
static std::shared_ptr<skity::Pixmap> g_image_mandrill;

static constexpr float g_case_size = 128;
static float dx = -g_case_size;
static float dy = 0;
static void update_dx_dy(bool update = true) {
  if (!update) {
    return;
  }
  dx += g_case_size;
  if ((dx + g_case_size) > kExampleWidth) {
    dx = 0;
    dy += g_case_size;
  }
}
static void reset_dx_dy() {
  dx = -g_case_size;
  dy = 0;
}

enum FilterType : int32_t {
  kIdentify_ImageFilter,
  kBlur_ImageFilter,
  kDropShadow_ImageFilter,
  kDilate_ImageFilter,
  kErode_ImageFilter,
  kMatrix_ImageFilter,
  kColorFilter_ImageFilter,
  kCompose_ImageFilter,
  kNormal_BlurMaskFilter,
};

enum DrawType : int32_t {
  kRect_DrawType,
  kImage_DrawType,
  kText_DrawType,
  kCircle_DrawType,
};

struct FilterTestContext {
  bool update_dx_dy = true;
  FilterType filter_type = kIdentify_ImageFilter;
  DrawType draw_type = kImage_DrawType;
  skity::Canvas* canvas = nullptr;
  std::shared_ptr<skity::Pixmap> image = nullptr;
  float xy = 0;
  float out_size = 0;
  float in_size = 0;
  float radius_x = 0;
  float radius_y = 0;
  float stroke_width = 0;
  skity::Color out_color = skity::Color_RED;
  skity::Rect fill_rect_on_start = skity::Rect();
  skity::Color fill_color_on_start = skity::Color_TRANSPARENT;
};

static std::shared_ptr<skity::Pixmap> load_image(const char* path) {
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

static void init_resources(skity::Canvas*) {
  if (g_image_firefox) {
    return;
  }
  g_image_firefox = load_image(EXAMPLE_IMAGE_ROOT "/firefox_64.png");
  g_image_mandrill = load_image(EXAMPLE_IMAGE_ROOT "/mandrill_128.png");
  return;
}

static const float kBLUR_SIGMA_SCALE = 0.57735f;
static float ConvertRadiusToSigma(float radius) {
  return radius > 0 ? kBLUR_SIGMA_SCALE * radius + 0.5f : 0.0f;
}

static void draw_blur_dilate_erode(FilterTestContext& ctx) {
  update_dx_dy(ctx.update_dx_dy);
  ctx.canvas->Save();
  ctx.canvas->Translate(dx, dy);

  auto sigma_x = ConvertRadiusToSigma(ctx.radius_x);
  auto sigma_y = ConvertRadiusToSigma(ctx.radius_y);
  auto radius = std::max(ctx.radius_x, ctx.radius_y);

  skity::Paint paint;

  if (!ctx.fill_rect_on_start.IsEmpty()) {
    paint.SetColor(ctx.fill_color_on_start);
    ctx.canvas->DrawRect(ctx.fill_rect_on_start, paint);
  }

  auto in_color = skity::Color_BLUE;
  paint.SetColor(in_color);

  if (ctx.stroke_width > 0) {
    paint.SetStyle(skity::Paint::kStroke_Style);
    paint.SetStrokeWidth(ctx.stroke_width);
  }

  if (ctx.filter_type == kBlur_ImageFilter) {
    paint.SetImageFilter(skity::ImageFilters::Blur(sigma_x, sigma_y));
  } else if (ctx.filter_type == kDropShadow_ImageFilter) {
    paint.SetImageFilter(skity::ImageFilters::DropShadow(
        64, 64, sigma_x, sigma_y, skity::Color_GREEN, nullptr));
  } else if (ctx.filter_type == kDilate_ImageFilter) {
    paint.SetImageFilter(
        skity::ImageFilters::Dilate(ctx.radius_x, ctx.radius_y));
  } else if (ctx.filter_type == kErode_ImageFilter) {
    paint.SetImageFilter(
        skity::ImageFilters::Erode(ctx.radius_x, ctx.radius_y));
  } else if (ctx.filter_type == kNormal_BlurMaskFilter) {
    paint.SetMaskFilter(
        skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, radius));
  }

  if (ctx.draw_type == kRect_DrawType) {
    ctx.canvas->DrawRect(
        skity::Rect::MakeXYWH(ctx.xy, ctx.xy, ctx.in_size, ctx.in_size), paint);
  } else if (ctx.draw_type == kImage_DrawType) {
    auto image = ctx.image;
    if (!image) {
      image =
          make_rect_image(ctx.out_size, ctx.out_color, ctx.in_size, in_color);
    }
    ctx.canvas->DrawImage(
        skity::Image::MakeImage(image),
        skity::Rect::MakeXYWH(ctx.xy, ctx.xy, image->Width(), image->Height()),
        &paint);
  } else if (ctx.draw_type == kText_DrawType) {
    paint.SetTextSize(ctx.in_size * 2);
    ctx.canvas->DrawSimpleText2("y", ctx.xy, ctx.in_size * 2, paint);
  } else if (ctx.draw_type == kCircle_DrawType) {
    ctx.canvas->DrawCircle(ctx.xy + ctx.out_size * 0.5,
                           ctx.xy + ctx.out_size * 0.5, ctx.in_size, paint);
  }

  ctx.canvas->Restore();
}

void draw_filter_example(skity::Canvas* canvas) {
  canvas->DrawColor(skity::ColorSetARGB(0x80, 0x00, 0x80, 0x80),
                    skity::BlendMode::kSrc);

  skity::Paint paint;
  float case_size = g_case_size;

  reset_dx_dy();
  init_resources(canvas);

  FilterTestContext ctx;
  ctx.canvas = canvas;

  if (g_debug) {
    ctx.xy = ctx.out_size = ctx.in_size = ctx.radius_x = ctx.radius_y = 1;
    ctx.draw_type = kImage_DrawType;
    // ctx.filter_type = kDilate_ImageFilter; draw_blur_dilate_erode(ctx);

    ctx.xy = 2;
    ctx.out_size = 3;
    ctx.in_size = ctx.radius_x = ctx.radius_y = 1;
    ctx.draw_type = kImage_DrawType;
    // ctx.filter_type = kErode_ImageFilter; draw_blur_dilate_erode(ctx);

    ctx.xy = 0;
    ctx.out_size = case_size * 0.5;
    ctx.in_size = case_size * 0.25;
    ctx.radius_x = ctx.radius_y = 1.5;
    ctx.draw_type = kImage_DrawType;
    ctx.filter_type = kDropShadow_ImageFilter;
    ctx.image = g_image_firefox;
    draw_blur_dilate_erode(ctx);
    return;
  }

  ctx.xy = case_size * 0.25;
  ctx.out_size = case_size * 0.5;
  ctx.in_size = case_size * 0.25;
  ctx.radius_x = ctx.radius_y = case_size * 0.125;
  ctx.draw_type = kImage_DrawType;

  ctx.filter_type = kIdentify_ImageFilter;
  draw_blur_dilate_erode(ctx);

  ctx.filter_type = kNormal_BlurMaskFilter;
  draw_blur_dilate_erode(ctx);

  ctx.filter_type = kBlur_ImageFilter;
  draw_blur_dilate_erode(ctx);

  ctx.image = g_image_firefox;

  ctx.filter_type = kIdentify_ImageFilter;
  draw_blur_dilate_erode(ctx);

  ctx.filter_type = kNormal_BlurMaskFilter;
  draw_blur_dilate_erode(ctx);

  ctx.filter_type = kBlur_ImageFilter;
  draw_blur_dilate_erode(ctx);

  ctx.image = nullptr;

  ctx.xy = 0;

  ctx.filter_type = kDropShadow_ImageFilter;
  draw_blur_dilate_erode(ctx);

  ctx.filter_type = kDropShadow_ImageFilter;
  ctx.fill_rect_on_start = skity::Rect(0, 0, case_size, case_size);
  ctx.fill_color_on_start = skity::ColorSetARGB(0xff, 0x80, 0x80, 0x00);
  ctx.out_color = skity::Color_TRANSPARENT;
  draw_blur_dilate_erode(ctx);

  ctx.filter_type = kDropShadow_ImageFilter;
  ctx.fill_rect_on_start = skity::Rect(0, 0, case_size, case_size);
  ctx.fill_color_on_start = skity::ColorSetARGB(0xff, 0x80, 0x00, 0x80);
  ctx.image = g_image_firefox;
  ctx.radius_x = ctx.radius_y = case_size * 0.025;
  draw_blur_dilate_erode(ctx);

  ctx.out_color = skity::Color_RED;
  ctx.fill_rect_on_start = {};
  ctx.xy = case_size * 0.25;
  ctx.image = nullptr;
  ctx.radius_x = ctx.radius_y = case_size * 0.125;

  ctx.filter_type = kDilate_ImageFilter;
  draw_blur_dilate_erode(ctx);

  ctx.filter_type = kErode_ImageFilter;
  draw_blur_dilate_erode(ctx);

  ctx.draw_type = kRect_DrawType;
  ctx.stroke_width = 2;
  ctx.radius_x = ctx.radius_y = case_size * 0.025;
  ctx.filter_type = kDilate_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.stroke_width = 0;

  ctx.radius_x = ctx.radius_y = 1.5;
  ctx.draw_type = kText_DrawType;

  ctx.xy = 0;
  ctx.filter_type = kIdentify_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.update_dx_dy = false;
  ctx.xy = case_size * 0.3;
  ctx.filter_type = kDilate_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.xy = case_size * 0.6;
  ctx.filter_type = kErode_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.update_dx_dy = true;

  ctx.xy = 0;
  ctx.filter_type = kDropShadow_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.update_dx_dy = false;
  ctx.xy = case_size * 0.3;
  ctx.filter_type = kNormal_BlurMaskFilter;
  draw_blur_dilate_erode(ctx);
  ctx.xy = case_size * 0.6;
  ctx.filter_type = kBlur_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.update_dx_dy = true;

  ctx.stroke_width = 2;

  ctx.xy = 0;
  ctx.filter_type = kIdentify_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.update_dx_dy = false;
  ctx.xy = case_size * 0.3;
  ctx.filter_type = kDilate_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.xy = case_size * 0.6;
  ctx.filter_type = kErode_ImageFilter;
  draw_blur_dilate_erode(ctx);
  ctx.update_dx_dy = true;

  ctx.stroke_width = 0;
}

}  // namespace skity::example::image_filter
