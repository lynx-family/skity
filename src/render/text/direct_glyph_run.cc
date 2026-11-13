// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/direct_glyph_run.hpp"

#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/text/atlas_glyph_run.hpp"
#include "src/render/text/glyph_position.hpp"

namespace skity {
namespace {

class DirectGlyphRun final : public AtlasGlyphRun {
 public:
  DirectGlyphRun(Atlas& atlas, AtlasGlyphBatch batch, const Paint& paint,
                 bool is_stroke, TextAtlasEffect effect,
                 const Matrix& local_to_raster, const Point& origin)
      : AtlasGlyphRun(atlas, std::move(batch), paint, is_stroke, effect),
        local_to_raster_(local_to_raster),
        origin_(origin) {}

 private:
  std::optional<AtlasGlyphDrawParams> GetDrawParams(
      const GlyphDrawContext& context) const override {
    const Matrix text_transform =
        local_to_raster_ * Matrix::Translate(origin_.x, origin_.y);
    const Matrix quad_transform =
        HWDynamicTextDraw::CalcTransform(local_to_raster_, text_transform);
    return AtlasGlyphDrawParams{
        context.raster_to_layer, quad_transform,
        context.raster_to_layer * local_to_raster_,
        (context.raster_to_layer * quad_transform).MapRect(GeometryBounds()),
        context.linear_filter ? GPUFilterMode::kLinear
                              : GPUFilterMode::kNearest};
  }

  const Matrix local_to_raster_;
  const Point origin_;
};

}  // namespace

void AppendDirectGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                           AtlasFormat format, bool is_stroke,
                           const GlyphRunBuildContext& context,
                           GlyphSubRunList& runs) {
  std::vector<const GlyphData*> metrics(input.count);
  Paint metrics_paint;
  if (is_stroke) {
    metrics_paint.SetStyle(Paint::kStroke_Style);
    metrics_paint.SetStrokeWidth(paint.GetStrokeWidth());
    metrics_paint.SetStrokeCap(paint.GetStrokeCap());
    metrics_paint.SetStrokeJoin(paint.GetStrokeJoin());
    metrics_paint.SetStrokeMiter(paint.GetStrokeMiter());
  }
  input.font.LoadGlyphMetrics(input.glyphs, input.count, metrics.data(),
                              metrics_paint);

  const Vec2 local_origin{input.origin.x, input.origin.y};
  const Vec2 zero{0.f, 0.f};
  Vec2 device_origin{}, device_zero{};
  context.local_to_raster.MapPoints(&device_origin, &local_origin, 1);
  context.local_to_raster.MapPoints(&device_zero, &zero, 1);
  const Vec2 origin_offset = device_origin - device_zero;
  GlyphPositionRoundingSpec rounding_spec;
  rounding_spec.is_subpixel = input.font.IsSubpixel();
  rounding_spec.axis_alignment = ComputeAxisAlignmentForHorizontalText(
      input.font.IsBaselineSnap(), context.local_to_raster);

  Atlas& atlas = *context.atlas_manager.GetAtlas(format);
  AtlasGlyphBatchBuilder batches(atlas);
  for (uint32_t index = 0; index < input.count; ++index) {
    if (metrics[index] == nullptr) {
      continue;
    }
    const Vec2 local_position{input.position_x[index] + input.origin.x,
                              input.position_y[index] + input.origin.y};
    Vec2 raster_position{};
    context.local_to_raster.MapPoints(&raster_position, &local_position, 1);
    const auto position = QuantizeGlyphPosition(
        raster_position, context.device_scale, rounding_spec);
    const GlyphRegion region = atlas.GetGlyphRegion(
        input.font,
        PackedGlyphID(input.glyphs[index], position.x_phase, position.y_phase),
        paint, false, context.device_scale, context.local_to_raster);
    const Vec2 top_left = position.position - origin_offset +
                          Vec2{region.origin_x, -region.origin_y};
    batches.Append(region, ResolveAtlasGlyphFormat(format, *metrics[index]),
                   Rect::MakeXYWH(top_left.x, top_left.y,
                                  region.loc.z / context.device_scale,
                                  region.loc.w / context.device_scale));
  }

  for (auto& batch : batches.TakeBatches()) {
    const auto effect = MaskAtlasEffect(batch.format);
    runs.push_back(context.arena.Make<DirectGlyphRun>(
        atlas, std::move(batch), paint, is_stroke, effect,
        context.local_to_raster, input.origin));
  }
}

}  // namespace skity
