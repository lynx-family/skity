// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/glyph_run.hpp"

#include <algorithm>
#include <cmath>

#include "src/render/text/direct_glyph_run.hpp"
#include "src/render/text/path_glyph_run.hpp"
#include "src/render/text/sdf_glyph_run.hpp"
#include "src/render/text/text_render_control.hpp"
#include "src/render/text/transformed_mask_glyph_run.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace {

enum class RasterMethod { kDirect, kSDF, kPath, kTransformedMask };

RasterMethod SelectRasterMethod(const GlyphRunInput& input, const Paint& paint,
                                AtlasFormat format,
                                const GlyphRunBuildContext& context) {
  const Matrix& transform = context.local_to_raster;
  const float scale_x =
      Vec2{transform.GetScaleX(), transform.GetSkewY()}.Length();
  const float scale_y =
      Vec2{transform.GetSkewX(), transform.GetScaleY()}.Length();
  const float text_scale = std::max(scale_x, scale_y) * context.device_scale;
  const float text_size = input.font.GetSize() * text_scale;

  if (format == AtlasFormat::RGBA32 &&
      context.atlas_manager.IsLargeEmojiAtlasEnabled() &&
      !transform.HasPersp()) {
    constexpr float kMaxLargeEmojiTextSize = 1024.f;
    if (text_size >= paint.GetFontThreshold() &&
        text_size < kMaxLargeEmojiTextSize &&
        context.atlas_manager.GetAtlas(format)
            ->GetConfig()
            .is_large_emoji_atlas) {
      return RasterMethod::kDirect;
    }
  }

  // Keep the existing policy: SDF support is implemented, but disabled here.
  TextRenderControl control{true};
  if (control.CanUseDirect(text_size, transform, paint,
                           input.font.GetTypeface())) {
    return RasterMethod::kDirect;
  }
  if (transform.HasPersp()) {
    return format == AtlasFormat::RGBA32 ? RasterMethod::kTransformedMask
                                         : RasterMethod::kPath;
  }
  if (control.CanUseSDF(text_scale, paint, input.font.GetTypeface())) {
    return RasterMethod::kSDF;
  }
  return RasterMethod::kPath;
}

void AppendDirectPaintPasses(const GlyphRunInput& input, const Paint& paint,
                             AtlasFormat format,
                             const GlyphRunBuildContext& context,
                             GlyphSubRunList& runs) {
  auto append_pass = [&](bool stroke) {
    Paint pass_paint = paint;
    pass_paint.SetStyle(stroke ? Paint::kStroke_Style : Paint::kFill_Style);
    AppendDirectGlyphRuns(input, pass_paint, format, stroke, context, runs);
  };

  if (format == AtlasFormat::RGBA32) {
    append_pass(false);
    return;
  }
  switch (paint.GetStyle()) {
    case Paint::kFill_Style:
      append_pass(false);
      break;
    case Paint::kStroke_Style:
      append_pass(true);
      break;
    case Paint::kStrokeAndFill_Style:
      append_pass(false);
      append_pass(true);
      break;
    case Paint::kStrokeThenFill_Style:
      append_pass(true);
      append_pass(false);
      break;
  }
}

void AppendPathGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                         const GlyphRunBuildContext& context,
                         GlyphSubRunList& runs) {
  std::vector<const GlyphData*> glyph_data(input.count);
  input.font.LoadGlyphPath(input.glyphs, input.count, glyph_data.data());
  auto has_outline = [&](uint32_t index) {
    return glyph_data[index] != nullptr &&
           !glyph_data[index]->GetPath().IsEmpty();
  };

  // Keep outline and bitmap-only spans in input order, including under
  // perspective where they use different representations.
  uint32_t begin = 0;
  while (begin < input.count) {
    const bool outline = has_outline(begin);
    uint32_t end = begin + 1;
    while (end < input.count && has_outline(end) == outline) {
      ++end;
    }
    if (outline) {
      std::vector<PositionedGlyphPath> paths;
      paths.reserve(end - begin);
      for (uint32_t index = begin; index < end; ++index) {
        paths.push_back(
            {glyph_data[index]->GetPath().CopyWithMatrix(
                 Matrix::Translate(input.origin.x, input.origin.y)),
             Vec2{input.position_x[index], input.position_y[index]}});
      }
      runs.push_back(context.arena.Make<PathGlyphRun>(std::move(paths), paint));
    } else if (context.local_to_raster.HasPersp()) {
      // Bitmap-only glyphs have no outline to stroke. Preserve the existing
      // fallback of one fill mask for all paint styles.
      transformed_mask::AppendGlyphRuns(
          input.Slice(begin, end - begin),
          transformed_mask::MakeBitmapOnlyFillPaint(paint), AtlasFormat::A8,
          false, context, runs);
    }
    begin = end;
  }
}

void AppendGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                     AtlasFormat format, const GlyphRunBuildContext& context,
                     GlyphSubRunList& runs) {
  switch (SelectRasterMethod(input, paint, format, context)) {
    case RasterMethod::kDirect:
      AppendDirectPaintPasses(input, paint, format, context, runs);
      break;
    case RasterMethod::kSDF:
      AppendSDFGlyphRuns(input, paint, context, runs);
      break;
    case RasterMethod::kPath:
      AppendPathGlyphRuns(input, paint, context, runs);
      break;
    case RasterMethod::kTransformedMask: {
      Paint fill_paint = paint;
      fill_paint.SetStyle(Paint::kFill_Style);
      transformed_mask::AppendGlyphRuns(input, fill_paint, format, false,
                                        context, runs);
      break;
    }
  }
}

}  // namespace

GlyphSubRunList GlyphRunBuilder::Build(const GlyphRunInput& input,
                                       const Paint& paint,
                                       const GlyphRunBuildContext& context) {
  SKITY_TRACE_EVENT(GlyphRunBuilder_Build);
  GlyphSubRunList runs;
  runs.SetArenaAllocator(&context.arena);
  if (input.count == 0 || input.glyphs == nullptr ||
      input.position_x == nullptr || input.position_y == nullptr ||
      input.font.GetTypeface() == nullptr ||
      !context.local_to_raster.IsFinite() ||
      !std::isfinite(context.device_scale) || context.device_scale <= 0.f) {
    return runs;
  }

  std::vector<const GlyphData*> metrics(input.count);
  Paint metrics_paint;
  metrics_paint.SetStyle(Paint::kFill_Style);
  input.font.LoadGlyphMetrics(input.glyphs, input.count, metrics.data(),
                              metrics_paint);
  auto atlas_format = [&](uint32_t index) {
    return metrics[index]->GetFormat() == GlyphFormat::A8 ? AtlasFormat::A8
                                                          : AtlasFormat::RGBA32;
  };

  uint32_t begin = 0;
  while (begin < input.count) {
    if (metrics[begin] == nullptr) {
      ++begin;
      continue;
    }
    const AtlasFormat format = atlas_format(begin);
    uint32_t end = begin + 1;
    while (end < input.count && metrics[end] != nullptr &&
           atlas_format(end) == format) {
      ++end;
    }
    AppendGlyphRuns(input.Slice(begin, end - begin), paint, format, context,
                    runs);
    begin = end;
  }
  return runs;
}

}  // namespace skity
