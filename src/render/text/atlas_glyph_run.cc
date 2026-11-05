// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas_glyph_run.hpp"

#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/tracing.hpp"

namespace skity {

void AtlasGlyphBatchBuilder::Append(const GlyphRegion& region,
                                    GlyphFormat format, const Rect& bounds) {
  if (region.loc.z <= 0 || region.loc.w <= 0 || bounds.IsEmpty() ||
      !bounds.IsFinite()) {
    return;
  }

  const uint32_t group_size = atlas_.GetConfig().max_num_bitmap_per_atlas;
  const uint32_t group_index = region.index_in_group / group_size;
  const uint32_t bitmap_index = region.index_in_group % group_size;
  if (batches_.empty() || batches_.back().group_index != group_index ||
      batches_.back().format != format) {
    batches_.push_back({group_index, format, {}});
  }

  const Vec2 uv_lt =
      atlas_.CalculateUV(bitmap_index, region.loc.x, region.loc.y);
  const Vec2 uv_rb = atlas_.CalculateUV(
      bitmap_index, region.loc.x + region.loc.z, region.loc.y + region.loc.w);
  auto& batch = batches_.back();
  batch.quads.emplace_back(
      Vec4{bounds.Left(), bounds.Top(), bounds.Right(), bounds.Bottom()}, uv_lt,
      uv_rb);
  batch.bounds.Join(bounds);
}

AtlasGlyphRun::AtlasGlyphRun(Atlas& atlas, AtlasGlyphBatch batch, Paint paint,
                             bool is_stroke, TextAtlasEffect effect)
    : atlas_(atlas),
      batch_(std::move(batch)),
      paint_(std::move(paint)),
      is_stroke_(is_stroke),
      effect_(effect) {}

void AtlasGlyphRun::Record(const GlyphDrawContext& context,
                           GlyphDrawTarget& target) const {
  SKITY_TRACE_EVENT(AtlasGlyphRun_Record);
  const auto params = GetDrawParams(context);
  if (!params || batch_.quads.empty()) {
    return;
  }

  ArrayList<GlyphRect, 16> quads;
  quads.SetArenaAllocator(&context.arena);
  for (const auto& quad : batch_.quads) {
    quads.push_back(quad);
  }

  atlas_.UploadAtlas(batch_.group_index);
  auto textures = atlas_.GetGPUTexture(batch_.group_index);
  auto sampler = atlas_.GetGPUSampler(batch_.group_index, params->filter);

  Paint draw_paint = paint_;
  if (effect_ != TextAtlasEffect::kSDFCoverage) {
    const auto color =
        is_stroke_ ? paint_.GetStrokeColor() : paint_.GetFillColor();
    draw_paint.SetFillColor(color);
    draw_paint.SetStrokeColor(color);
  }
  const bool has_coverage = effect_ == TextAtlasEffect::kA8Coverage ||
                            effect_ == TextAtlasEffect::kSDFCoverage;
  std::optional<Matrix> layer_to_local;
  if (has_coverage && paint_.GetShader()) {
    Matrix inverse;
    params->local_to_layer.InvertZ0Plane(&inverse);
    layer_to_local = inverse;
  }

  auto* draw = context.arena.Make<HWDynamicTextDraw>(
      params->draw_transform, params->quad_transform, std::move(quads),
      draw_paint, textures, sampler, effect_, is_stroke_, layer_to_local);
  target.SubmitAtlas({draw, paint_.ComputeFastBounds(params->layer_bounds),
                      paint_.GetBlendMode(), has_coverage,
                      has_coverage && IsPaintSourceOpaque(paint_)});
}

GlyphFormat ResolveAtlasGlyphFormat(AtlasFormat format,
                                    const GlyphData& glyph) {
  if (format == AtlasFormat::A8) {
    return GlyphFormat::A8;
  }
  return glyph.GetFormat() == GlyphFormat::BGRA32 ? GlyphFormat::BGRA32
                                                  : GlyphFormat::RGBA32;
}

TextAtlasEffect MaskAtlasEffect(GlyphFormat format) {
  switch (format) {
    case GlyphFormat::A8:
      return TextAtlasEffect::kA8Coverage;
    case GlyphFormat::BGRA32:
      return TextAtlasEffect::kColorSwizzleRB;
    default:
      return TextAtlasEffect::kColor;
  }
}

}  // namespace skity
