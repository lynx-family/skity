// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/sdf_glyph_run.hpp"

#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/text/atlas_glyph_run.hpp"

namespace skity {
namespace {

class SDFGlyphRun final : public AtlasGlyphRun {
 public:
  SDFGlyphRun(Atlas& atlas, AtlasGlyphBatch batch, const Paint& paint,
              const Matrix& local_to_raster, const Point& origin)
      : AtlasGlyphRun(atlas, std::move(batch), paint, false,
                      TextAtlasEffect::kSDFCoverage),
        local_to_raster_(local_to_raster),
        origin_(origin) {}

 private:
  std::optional<AtlasGlyphDrawParams> GetDrawParams(
      const GlyphDrawContext& context) const override {
    const Matrix text_transform =
        local_to_raster_ * Matrix::Translate(origin_.x, origin_.y);
    const Matrix quad_transform =
        HWDynamicTextDraw::CalcSDFTransform(text_transform, 1.f);
    // The draw maps each quad to an AABB before the GPU applies
    // raster_to_layer.
    return AtlasGlyphDrawParams{context.raster_to_layer, quad_transform,
                                context.raster_to_layer * local_to_raster_,
                                context.raster_to_layer.MapRect(
                                    quad_transform.MapRect(GeometryBounds())),
                                GPUFilterMode::kLinear};
  }

  const Matrix local_to_raster_;
  const Point origin_;
};

}  // namespace

void AppendSDFGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                        const GlyphRunBuildContext& context,
                        GlyphSubRunList& runs) {
  std::vector<const GlyphData*> metrics(input.count);
  input.font.LoadGlyphMetrics(input.glyphs, input.count, metrics.data(), paint);
  Atlas& atlas = *context.atlas_manager.GetAtlas(AtlasFormat::A8);
  AtlasGlyphBatchBuilder batches(atlas);
  const float fixed_size = input.font.GetFixedSize();
  for (uint32_t index = 0; index < input.count; ++index) {
    if (metrics[index] == nullptr) {
      continue;
    }
    const GlyphData& info = *metrics[index];
    const auto result = atlas.GetGlyphRegion(
        input.font, PackedGlyphID(input.glyphs[index]), paint, true,
        context.device_scale, context.local_to_raster);
    if (!result.IsReady()) {
      continue;
    }
    const auto& region = result.region;
    float width = region.loc.z * region.scale / context.device_scale;
    float height = region.loc.w * region.scale / context.device_scale;
    if (fixed_size != 0.f) {
      const float scale =
          input.font.GetSize() * context.device_scale / fixed_size;
      width *= scale;
      height *= scale;
    }
    batches.Append(
        region, GlyphFormat::A8,
        Rect::MakeXYWH(input.position_x[index] + info.GetHoriBearingX(),
                       input.position_y[index] - info.GetHoriBearingY(), width,
                       height));
  }

  for (auto& batch : batches.TakeBatches()) {
    runs.push_back(context.arena.Make<SDFGlyphRun>(
        atlas, std::move(batch), paint, context.local_to_raster, input.origin));
  }
}

}  // namespace skity
