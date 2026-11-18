// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/drawable_glyph_run.hpp"

#include <cmath>
#include <utility>

#include "src/text/scaler_context_cache.hpp"
#include "src/tracing.hpp"

namespace skity::drawable {
namespace {

struct PositionedDrawable {
  std::shared_ptr<const GlyphDrawable> drawable;
  Vec2 position;
};

class DrawableGlyphRun final : public GlyphSubRun {
 public:
  DrawableGlyphRun(std::vector<PositionedDrawable> glyphs, float scale,
                   Paint paint)
      : glyphs_(std::move(glyphs)), scale_(scale), paint_(std::move(paint)) {}

  void Record(const GlyphDrawContext& context,
              GlyphDrawTarget& target) const override {
    SKITY_TRACE_EVENT(DrawableGlyphRun_Record);
    for (const auto& glyph : glyphs_) {
      Matrix matrix = Matrix::Translate(glyph.position.x, glyph.position.y) *
                      Matrix::Scale(scale_, scale_);
      if (matrix.IsFinite() && context.local_to_layer.IsFinite()) {
        target.SubmitDrawable(*glyph.drawable, paint_, matrix,
                              context.local_to_layer);
      }
    }
  }

 private:
  const std::vector<PositionedDrawable> glyphs_;
  const float scale_;
  const Paint paint_;
};

}  // namespace

std::vector<std::shared_ptr<const GlyphDrawable>> PrepareGlyphs(
    const GlyphRunInput& input, const Paint& paint) {
  std::vector<std::shared_ptr<const GlyphDrawable>> glyphs(input.count);
  if (!std::isfinite(input.font.GetSize()) || input.font.GetSize() <= 0.f) {
    return glyphs;
  }
  Font strike_font = input.font;
  strike_font.SetSize(kStrikeSize);
  strike_font.SetHinting(Font::FontHinting::kNone);
  strike_font.SetForceAutoHinting(false);
  strike_font.SetEmbeddedBitmaps(false);
  strike_font.SetSubpixel(false);
  strike_font.SetBaselineSnap(false);
  strike_font.SetLinearMetrics(true);
  strike_font.SetEdging(Font::Edging::kAntiAlias);
  Paint strike_paint;
  // Foreground RGB belongs to the recorded glyph; overall alpha is applied
  // once at submission, after the glyph's own layers have been composited.
  strike_paint.SetColor(ColorSetA(Color4fToColor(paint.GetFillColor()), 255));
  const auto desc = ScalerContextDesc::MakeTransformed(
      strike_font, strike_paint, 1.f, Matrix22{});
  auto scaler =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, input.font.GetTypeface());
  if (scaler) {
    scaler->PrepareDrawables(input.glyphs, input.count, glyphs.data());
  }
  return glyphs;
}

void AppendGlyphRuns(const GlyphRunInput& input,
                     const std::shared_ptr<const GlyphDrawable>* glyphs,
                     const Paint& paint, const GlyphRunBuildContext& context,
                     GlyphSubRunList& runs) {
  std::vector<PositionedDrawable> positioned;
  positioned.reserve(input.count);
  for (uint32_t i = 0; i < input.count; ++i) {
    const Vec2 position{input.origin.x + input.position_x[i],
                        input.origin.y + input.position_y[i]};
    if (glyphs[i] && std::isfinite(position.x) && std::isfinite(position.y)) {
      positioned.push_back({glyphs[i], position});
    }
  }
  if (positioned.empty()) {
    return;
  }
  Paint layer_paint = paint;
  layer_paint.SetStyle(Paint::kFill_Style);
  layer_paint.SetShader(nullptr);
  layer_paint.SetPathEffect(nullptr);
  runs.push_back(context.arena.Make<DrawableGlyphRun>(
      std::move(positioned), input.font.GetSize() / kStrikeSize, layer_paint));
}

}  // namespace skity::drawable
