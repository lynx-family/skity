// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/color_space/color_space_example.hpp"

#include <cstdio>
#include <memory>
#include <skity/codec/codec.hpp>
#include <skity/effect/shader.hpp>
#include <skity/skity.hpp>

namespace skity::example::color_space {

namespace {

// A row of saturated colors: pure channels and mixes show the largest
// differences between gamuts and transfer functions.
constexpr Color kSwatchColors[] = {
    ColorSetARGB(255, 255, 0, 0),   ColorSetARGB(255, 0, 255, 0),
    ColorSetARGB(255, 0, 0, 255),   ColorSetARGB(255, 255, 255, 0),
    ColorSetARGB(255, 255, 0, 255), ColorSetARGB(255, 0, 255, 255),
};
constexpr size_t kSwatchCount = sizeof(kSwatchColors) / sizeof(Color);

// Builds a w x h unpremul RGBA image filled with `color`, tagged with
// `space`.
std::shared_ptr<Image> MakeTaggedImage(uint32_t w, uint32_t h, Color color,
                                       ColorSpace space) {
  auto pixmap = std::make_shared<Pixmap>(w, h);
  Bitmap bitmap(pixmap, false);
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      bitmap.SetPixel(x, y, color);
    }
  }
  pixmap->SetColorInfo(kUnpremul_AlphaType, ColorType::kRGBA, space);
  return Image::MakeImage(pixmap);
}

// Panel 1: identical bytes, different declarations. The same swatch colors
// are stored three times and only the tag changes, so the visible
// differences come purely from the decode into the target space.
void DrawTaggedSwatchRows(Canvas* canvas) {
  struct {
    const char* name;
    ColorSpace space;
  } rows[] = {
      {"sRGB", ColorSpace{}},
      {"Display P3", ColorSpace::DisplayP3()},
      {"Adobe RGB", ColorSpace::AdobeRGB()},
      {"linear sRGB", ColorSpace::SRGBLinear()},
  };

  float y = 16.f;
  for (const auto& row : rows) {
    float x = 16.f;
    for (size_t i = 0; i < kSwatchCount; i++) {
      auto image = MakeTaggedImage(64, 64, kSwatchColors[i], row.space);
      canvas->DrawImage(image, Rect::MakeXYWH(x, y, 64.f, 64.f), nullptr);
      x += 72.f;
    }
    y += 72.f;
  }
}

// Panel 2: one color value recorded in different color spaces. The paint
// resolves the recorded space into the target space at draw time. Two rows:
// the moderate orange shows the transfer-function differences dominating
// (linear is much lighter; gamma 2.2 is nearly identical to sRGB because
// the sRGB curve IS a piecewise 2.2 power curve), while the saturated green
// shows the gamut difference (the Display P3 swatch drops the red tint its
// sRGB twin keeps, because the gamut matrix moves it out before clipping).
void DrawPaintSpaceSwatches(Canvas* canvas) {
  // A pure power curve over the sRGB gamut: only the transfer function
  // differs from sRGB.
  const float gamma22_tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto gamma22 = ColorSpace::MakeRGB(gamma22_tf, ColorSpace{}.gamut_xyz);

  ColorSpace spaces[5] = {ColorSpace{}, ColorSpace::SRGBLinear(),
                          ColorSpace::DisplayP3(), ColorSpace::AdobeRGB(),
                          gamma22 ? *gamma22 : ColorSpace{}};
  const Color4f row_colors[2] = {
      Color4f{1.f, 0.25f, 0.1f, 1.f},  // moderate orange
      Color4f{0.2f, 0.9f, 0.2f, 1.f},  // saturated green
  };

  for (size_t row = 0; row < 2; row++) {
    float y = 320.f + row * 80.f;
    float x = 16.f;
    for (const auto& space : spaces) {
      Paint paint;
      paint.SetColor4f(row_colors[row], space);
      canvas->DrawRect(Rect::MakeXYWH(x, y, 144.f, 72.f), paint);
      x += 156.f;
    }
  }
}

// Panel 3: wide gamut green primaries against sRGB green. The Adobe RGB
// green primary is the farthest outside sRGB of the built-in spaces, so
// the left swatch shows the most muted sRGB rendering, Display P3 sits in
// between, and the right swatch is sRGB green itself; the gaps are exactly
// the parts of each gamut that sRGB cannot represent.
void DrawPrimaryComparison(Canvas* canvas) {
  Paint adobe_green;
  adobe_green.SetColor4f(Color4f{0.f, 1.f, 0.f, 1.f}, ColorSpace::AdobeRGB());
  canvas->DrawRect(Rect::MakeXYWH(16.f, 488.f, 244.f, 112.f), adobe_green);

  Paint p3_green;
  p3_green.SetColor4f(Color4f{0.f, 1.f, 0.f, 1.f}, ColorSpace::DisplayP3());
  canvas->DrawRect(Rect::MakeXYWH(272.f, 488.f, 244.f, 112.f), p3_green);

  Paint srgb_green;
  srgb_green.SetColor4f(Color4f{0.f, 1.f, 0.f, 1.f}, ColorSpace{});
  canvas->DrawRect(Rect::MakeXYWH(528.f, 488.f, 256.f, 112.f), srgb_green);
}

// Panel 4: hues sampled around the Display P3 gamut. Every swatch is
// declared in P3; on an sRGB target each shows its closest sRGB rendering,
// which is what a wide gamut scene looks like when it is not clipped
// earlier in the pipeline.
void DrawP3HueStrip(Canvas* canvas) {
  const Color4f hues[] = {
      {1.f, 0.f, 0.f, 1.f},   {1.f, 0.25f, 0.f, 1.f}, {1.f, 0.5f, 0.f, 1.f},
      {0.75f, 1.f, 0.f, 1.f}, {0.f, 1.f, 0.25f, 1.f}, {0.f, 0.85f, 1.f, 1.f},
      {0.f, 0.3f, 1.f, 1.f},  {0.55f, 0.f, 1.f, 1.f},
  };
  constexpr size_t kHueCount = sizeof(hues) / sizeof(hues[0]);

  float x = 16.f;
  for (size_t i = 0; i < kHueCount; i++) {
    Paint paint;
    paint.SetColor4f(hues[i], ColorSpace::DisplayP3());
    canvas->DrawRect(Rect::MakeXYWH(x, 616.f, 89.f, 168.f), paint);
    x += 96.f;
  }
}

// The scene exported from the software backend, drawn into a Display P3
// tagged bitmap.
void DrawExportScene(Canvas* canvas) {
  canvas->Clear(Color_WHITE);

  DrawTaggedSwatchRows(canvas);
  DrawPaintSpaceSwatches(canvas);
  DrawPrimaryComparison(canvas);
  DrawP3HueStrip(canvas);
}

}  // namespace

void draw_color_space_example(Canvas* canvas) {
  canvas->Clear(Color_WHITE);

  // Top four rows: identical bytes tagged sRGB / Display P3 / Adobe RGB /
  // linear.
  DrawTaggedSwatchRows(canvas);
  // Two paint colors recorded in sRGB / linear / P3 / Adobe RGB / gamma 2.2
  // spaces: transfer-function differences dominate the orange row, gamut
  // differences dominate the green row.
  DrawPaintSpaceSwatches(canvas);
  // P3 red primary vs sRGB red: the visible gap is the wide gamut part.
  DrawPrimaryComparison(canvas);
  // P3 hues on an sRGB target: closest sRGB renderings, unclipped early.
  DrawP3HueStrip(canvas);
}

void export_display_p3_png() {
  constexpr uint32_t kWidth = 800;
  constexpr uint32_t kHeight = 800;

  Bitmap bitmap(kWidth, kHeight, AlphaType::kPremul_AlphaType);
  if (!bitmap.GetPixmap()->SetColorInfo(kPremul_AlphaType, ColorType::kRGBA,
                                        ColorSpace::DisplayP3())) {
    std::printf("color_space example: failed to tag the export bitmap\n");
    return;
  }

  auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  DrawExportScene(canvas.get());
  canvas->Flush();

  auto codec = Codec::MakePngCodec();
  auto data = codec->Encode(bitmap.GetPixmap().get());
  if (!data) {
    std::printf("color_space example: PNG encode failed\n");
    return;
  }

  const char* path = "color_space_display_p3.png";
  if (!data->WriteToFile(path)) {
    std::printf("color_space example: failed to write %s\n", path);
    return;
  }
  std::printf(
      "color_space example: exported %u x %u Display P3 PNG to %s "
      "(gAMA/cHRM chunks describe the gamut)\n",
      kWidth, kHeight, path);
}

}  // namespace skity::example::color_space
