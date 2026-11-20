// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/direct_glyph_run.hpp"

#include <algorithm>

#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/text/atlas_glyph_run.hpp"
#include "src/render/text/glyph_position.hpp"

namespace skity {
namespace {

bool HasOutline(const GlyphData* glyph) {
  return glyph != nullptr && !glyph->GetPath().IsEmpty();
}

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

std::vector<PreparedDirectGlyph> PrepareDirectGlyphs(
    const GlyphRunInput& input, const Paint& paint, AtlasFormat format,
    const GlyphRunBuildContext& context) {
  std::vector<PreparedDirectGlyph> glyphs(input.count);
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
  for (uint32_t index = 0; index < input.count; ++index) {
    const Vec2 local_position{input.position_x[index] + input.origin.x,
                              input.position_y[index] + input.origin.y};
    Vec2 raster_position{};
    context.local_to_raster.MapPoints(&raster_position, &local_position, 1);
    const auto position = QuantizeGlyphPosition(
        raster_position, context.device_scale, rounding_spec);
    auto& glyph = glyphs[index];
    glyph.atlas = atlas.GetGlyphRegion(
        input.font,
        PackedGlyphID(input.glyphs[index], position.x_phase, position.y_phase),
        paint, false, context.device_scale, context.local_to_raster);
    if (!glyph.atlas.IsReady()) {
      continue;
    }
    const auto& region = glyph.atlas.region;
    const Vec2 top_left = position.position - origin_offset +
                          Vec2{region.origin_x, -region.origin_y};
    glyph.bounds = Rect::MakeXYWH(top_left.x, top_left.y,
                                  region.loc.z / context.device_scale,
                                  region.loc.w / context.device_scale);
  }
  return glyphs;
}

void AppendPreparedDirectGlyphRuns(const GlyphRunInput& input,
                                   const PreparedDirectGlyph* glyphs,
                                   const Paint& paint, AtlasFormat format,
                                   bool is_stroke,
                                   const GlyphRunBuildContext& context,
                                   GlyphSubRunList& runs) {
  Atlas& atlas = *context.atlas_manager.GetAtlas(format);
  AtlasGlyphBatchBuilder batches(atlas);
  for (uint32_t index = 0; index < input.count; ++index) {
    const auto& glyph = glyphs[index];
    if (glyph.atlas.IsReady()) {
      batches.Append(glyph.atlas.region, glyph.atlas.region.format,
                     glyph.bounds);
    }
  }

  for (auto& batch : batches.TakeBatches()) {
    const auto effect = MaskAtlasEffect(batch.format);
    runs.push_back(context.arena.Make<DirectGlyphRun>(
        atlas, std::move(batch), paint, is_stroke, effect,
        context.local_to_raster, input.origin));
  }
}

void AppendDirectGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                           AtlasFormat format,
                           const GlyphRunBuildContext& context,
                           GlyphSubRunList& runs,
                           GlyphRunFallback append_fallback) {
  struct Pass {
    Paint paint;
    bool stroke;
    std::vector<PreparedDirectGlyph> glyphs;
  };
  std::vector<Pass> passes;
  auto prepare = [&](bool stroke) {
    Paint pass = paint;
    pass.SetStyle(stroke ? Paint::kStroke_Style : Paint::kFill_Style);
    passes.push_back(
        {pass, stroke, PrepareDirectGlyphs(input, pass, format, context)});
  };
  if (format == AtlasFormat::RGBA32) {
    prepare(false);
  } else {
    switch (paint.GetStyle()) {
      case Paint::kFill_Style:
        prepare(false);
        break;
      case Paint::kStroke_Style:
        prepare(true);
        break;
      case Paint::kStrokeAndFill_Style:
        prepare(false);
        prepare(true);
        break;
      case Paint::kStrokeThenFill_Style:
        prepare(true);
        prepare(false);
        break;
    }
  }

  std::vector<uint32_t> rejected_indices;
  std::vector<GlyphID> rejected_ids;
  for (uint32_t index = 0; index < input.count; ++index) {
    if (std::any_of(passes.begin(), passes.end(), [&](const Pass& pass) {
          return !pass.glyphs[index].atlas.IsReady();
        })) {
      rejected_indices.push_back(index);
      rejected_ids.push_back(input.glyphs[index]);
    }
  }
  if (rejected_indices.empty()) {
    for (const auto& pass : passes) {
      AppendPreparedDirectGlyphRuns(input, pass.glyphs.data(), pass.paint,
                                    format, pass.stroke, context, runs);
    }
    return;
  }

  // Decide all passes before emitting any geometry. A bitmap-only glyph
  // falling back from Stroke must not be drawn again over an accepted Fill.
  std::vector<bool> bitmap_fallback(input.count, false);
  std::vector<const GlyphData*> paths(input.count);
  if (format == AtlasFormat::A8) {
    std::vector<const GlyphData*> rejected_paths(rejected_ids.size());
    input.font.LoadGlyphPath(rejected_ids.data(), rejected_ids.size(),
                             rejected_paths.data());
    for (size_t i = 0; i < rejected_indices.size(); ++i) {
      paths[rejected_indices[i]] = rejected_paths[i];
    }
    for (uint32_t index = 0; index < input.count; ++index) {
      const bool fill_ready =
          std::any_of(passes.begin(), passes.end(), [&](const Pass& pass) {
            return !pass.stroke && pass.glyphs[index].atlas.IsReady();
          });
      for (const auto& pass : passes) {
        const auto status = pass.glyphs[index].atlas.status;
        bitmap_fallback[index] =
            bitmap_fallback[index] ||
            (!HasOutline(paths[index]) &&
             (status == AtlasGlyphStatus::kTooLarge ||
              (status == AtlasGlyphStatus::kNoImage && pass.stroke &&
               (passes.size() == 1 || fill_ready))));
      }
    }
  }

  enum class Choice { kDirect, kFallback, kFallbackWithoutImage, kSkip };
  for (const auto& pass : passes) {
    auto choice = [&](uint32_t index) {
      if (bitmap_fallback[index]) {
        return !pass.stroke || passes.size() == 1 ? Choice::kFallback
                                                  : Choice::kSkip;
      }
      const auto& result = pass.glyphs[index].atlas;
      if (result.IsReady()) {
        return Choice::kDirect;
      }
      if (result.status == AtlasGlyphStatus::kTooLarge ||
          HasOutline(paths[index])) {
        return Choice::kFallback;
      }
      if (format == AtlasFormat::RGBA32) {
        // A terminal image failure does not rule out a vector representation.
        // Let the fallback policy decide without retrying the failed image.
        return Choice::kFallbackWithoutImage;
      }
      return Choice::kSkip;
    };
    uint32_t begin = 0;
    while (begin < input.count) {
      const auto selected = choice(begin);
      uint32_t end = begin + 1;
      while (end < input.count && choice(end) == selected) {
        ++end;
      }
      const auto span = input.Slice(begin, end - begin);
      if (selected == Choice::kDirect) {
        AppendPreparedDirectGlyphRuns(span, pass.glyphs.data() + begin,
                                      pass.paint, format, pass.stroke, context,
                                      runs);
      } else if (selected == Choice::kFallback) {
        append_fallback(span, pass.paint, format, context, runs,
                        paths.data() + begin, true);
      }
      if (selected == Choice::kFallbackWithoutImage) {
        append_fallback(span, pass.paint, format, context, runs, nullptr,
                        false);
      }
      begin = end;
    }
  }
}

}  // namespace skity
