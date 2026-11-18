// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_UT_TEXT_GLYPH_TEST_TYPEFACE_HPP
#define TEST_UT_TEXT_GLYPH_TEST_TYPEFACE_HPP

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <skity/recorder/picture_recorder.hpp>
#include <vector>

// This accessor contains no platform APIs and lets the fixture supply metrics
// through the same internal setters as a font backend.
#include "src/text/glyph_drawable.hpp"
#include "src/text/ports/win/glyph_data_win_access.hpp"
#include "src/text/scaler_context.hpp"

namespace skity::test {

struct TestGlyphSpec {
  float width = 20.f;
  float height = 20.f;
  bool outline = false;
  GlyphFormat format = GlyphFormat::A8;
  float stroke_width = 0.f;
  bool fixed_image = false;
  bool invalid_image = false;
  float fail_below_scale = 0.f;
  bool unspecified_format = false;
  bool drawable = false;
};

class GlyphTestTypeface final : public Typeface {
 public:
  struct State {
    std::vector<TestGlyphSpec> glyphs;
    std::vector<uint32_t> image_requests;
    std::vector<uint32_t> drawable_requests;
    uint32_t path_requests = 0;
  };

  explicit GlyphTestTypeface(std::vector<TestGlyphSpec> glyphs)
      : Typeface(FontStyle{}), state(std::make_shared<State>()) {
    state->glyphs = std::move(glyphs);
    state->image_requests.resize(state->glyphs.size());
    state->drawable_requests.resize(state->glyphs.size());
  }
  std::shared_ptr<State> state;

 private:
  class Context final : public ScalerContext {
   public:
    Context(std::shared_ptr<State> state, const ScalerContextDesc* desc)
        : ScalerContext({}, desc), state_(std::move(state)) {}

   private:
    void GenerateMetrics(GlyphData* glyph) override {
      const auto& spec = state_->glyphs.at(glyph->Id());
      // Deliberately underestimate large images, as bitmap-only backends may
      // not expose an accurate lightweight bitmap-info path.
      const float size = desc_.text_size;
      GlyphDataWinAccess::SetMetrics(
          glyph, size, 0.f,
          Rect::MakeLTRB(-size * .15f, -size * .85f, size * .85f, size * .15f),
          spec.unspecified_format ? std::nullopt
                                  : std::optional<GlyphFormat>(spec.format));
    }
    void GenerateImage(PackedGlyphID id, GlyphData* glyph,
                       const StrokeDesc& stroke) override {
      const auto& spec = state_->glyphs.at(id.GetGlyphID());
      ++state_->image_requests[id.GetGlyphID()];
      const float scale = desc_.transform.GetScaleX() * desc_.text_size / 20.f;
      if (scale < spec.fail_below_scale || spec.width == 0.f ||
          spec.height == 0.f) {
        return;
      }
      const float raster_scale =
          spec.fixed_image ? 1.f : scale * desc_.context_scale;
      GlyphBitmapData image;
      image.width = std::ceil((stroke.is_stroke && spec.stroke_width > 0.f
                                   ? spec.stroke_width
                                   : spec.width) *
                              raster_scale);
      image.height = std::ceil(spec.height * raster_scale);
      image.origin_x = -3.f * scale;
      image.origin_y = 17.f * scale;
      image.format = spec.format == GlyphFormat::A8 ? BitmapFormat::kGray8
                     : spec.format == GlyphFormat::BGRA32
                         ? BitmapFormat::kBGRA8
                         : BitmapFormat::kRGBA8;
      const size_t bpp = spec.format == GlyphFormat::A8 ? 1 : 4;
      image.row_bytes = size_t(image.width) * bpp;
      image.buffer = static_cast<uint8_t*>(
          std::malloc(image.row_bytes * size_t(image.height)));
      image.need_free = true;
      for (size_t y = 0; y < size_t(image.height); ++y) {
        for (size_t x = 0; x < size_t(image.width); ++x) {
          auto* p = image.buffer + y * image.row_bytes + x * bpp;
          if (bpp == 1) {
            p[0] = 255;
          } else {
            const bool red = x < size_t(image.width) / 2;
            p[0] = red ? 255 : 0;
            p[1] = 0;
            p[2] = red ? 0 : 255;
            p[3] = 255;
          }
        }
      }
      if (spec.invalid_image) {
        image.row_bytes = 1;
      }
      GlyphDataWinAccess::SetImage(glyph, image);
    }
    void GenerateImageInfo(PackedGlyphID, GlyphData*,
                           const StrokeDesc&) override {}
    std::shared_ptr<const GlyphDrawable> GenerateDrawable(
        const GlyphData& glyph) override {
      ++state_->drawable_requests[glyph.Id()];
      const auto& spec = state_->glyphs.at(glyph.Id());
      if (!spec.drawable) {
        return nullptr;
      }
      const float scale = desc_.text_size / 20.f;
      Matrix matrix;
      matrix.SetScaleX(desc_.scale_x);
      matrix.SetSkewX(desc_.skew_x);
      const Rect rect = Rect::MakeXYWH(-3.f * scale, -17.f * scale,
                                       spec.width * scale, spec.height * scale);
      const Rect bounds = matrix.MapRect(rect);
      PictureRecorder recorder;
      recorder.BeginRecording(bounds);
      auto* canvas = recorder.GetRecordingCanvas();
      canvas->Concat(matrix);
      Paint red;
      red.SetColor(Color_RED);
      canvas->DrawRect(rect, red);
      Paint foreground;
      foreground.SetColor(desc_.foreground_color);
      canvas->DrawRect(Rect::MakeLTRB(rect.CenterX(), rect.Top(), rect.Right(),
                                      rect.Bottom()),
                       foreground);
      return GlyphDrawable::Make(recorder.FinishRecording(), bounds);
    }
    bool GeneratePath(GlyphData* glyph) override {
      ++state_->path_requests;
      const auto& spec = state_->glyphs.at(glyph->Id());
      if (!spec.outline) {
        return false;
      }
      const float scale = desc_.text_size / 20.f;
      Path path;
      path.AddRect(-3.f * scale, -17.f * scale, (spec.width - 3.f) * scale,
                   (spec.height - 17.f) * scale);
      GlyphDataWinAccess::SetPath(glyph, std::move(path));
      return true;
    }
    void GenerateFontMetrics(FontMetrics* metrics) override { *metrics = {}; }
    uint16_t OnGetFixedSize() override { return 0; }
    std::shared_ptr<State> state_;
  };

  int OnGetTableTags(FontTableTag*) const override { return 0; }
  size_t OnGetTableData(FontTableTag, size_t, size_t, void*) const override {
    return 0;
  }
  void OnCharsToGlyphs(const uint32_t* chars, int count,
                       GlyphID* glyphs) const override {
    for (int i = 0; i < count; ++i) {
      glyphs[i] = chars[i] % state->glyphs.size();
    }
  }
  std::shared_ptr<Data> OnGetData() override { return nullptr; }
  uint32_t OnGetUPEM() const override { return 1000; }
  bool OnContainsColorTable() const override {
    return std::any_of(state->glyphs.begin(), state->glyphs.end(),
                       [](const auto& glyph) { return glyph.drawable; });
  }
  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc* desc) const override {
    return std::make_unique<Context>(state, desc);
  }
  VariationPosition OnGetVariationDesignPosition() const override { return {}; }
  std::vector<VariationAxis> OnGetVariationDesignParameters() const override {
    return {};
  }
  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments&) const override {
    return nullptr;
  }
  void OnGetFontDescriptor(FontDescriptor&) const override {}
};

}  // namespace skity::test

#endif  // TEST_UT_TEXT_GLYPH_TEST_TYPEFACE_HPP
