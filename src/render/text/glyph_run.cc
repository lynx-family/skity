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

enum class RasterMethod { kDirect, kSDF, kFallback };

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
  if (control.CanUseSDF(text_scale, paint, input.font.GetTypeface())) {
    return RasterMethod::kSDF;
  }
  return RasterMethod::kFallback;
}

bool HasOutline(const GlyphData* glyph) {
  return glyph != nullptr && !glyph->GetPath().IsEmpty();
}

void AppendPaths(const GlyphRunInput& input, const GlyphData* const* glyphs,
                 const Paint& paint, const GlyphRunBuildContext& context,
                 GlyphSubRunList& runs) {
  std::vector<PositionedGlyphPath> paths;
  paths.reserve(input.count);
  for (uint32_t index = 0; index < input.count; ++index) {
    paths.push_back({glyphs[index]->GetPath().CopyWithMatrix(
                         Matrix::Translate(input.origin.x, input.origin.y)),
                     Vec2{input.position_x[index], input.position_y[index]}});
  }
  runs.push_back(context.arena.Make<PathGlyphRun>(std::move(paths), paint));
}

// One fallback policy for a rejected Direct image and a skipped Direct
// strategy: preserve color, otherwise prefer an outline, then a fill mask.
void AppendFallbackGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                             AtlasFormat format,
                             const GlyphRunBuildContext& context,
                             GlyphSubRunList& runs,
                             const GlyphData* const* prepared_paths = nullptr) {
  if (format == AtlasFormat::RGBA32) {
    Paint fill = paint;
    fill.SetStyle(Paint::kFill_Style);
    transformed_mask::AppendGlyphRuns(input, fill, format, false, context,
                                      runs);
    return;
  }
  std::vector<const GlyphData*> paths;
  if (!prepared_paths) {
    paths.resize(input.count);
    input.font.LoadGlyphPath(input.glyphs, input.count, paths.data());
    prepared_paths = paths.data();
  }
  uint32_t begin = 0;
  while (begin < input.count) {
    const bool outline = HasOutline(prepared_paths[begin]);
    uint32_t end = begin + 1;
    while (end < input.count && HasOutline(prepared_paths[end]) == outline) {
      ++end;
    }
    const auto span = input.Slice(begin, end - begin);
    if (outline) {
      AppendPaths(span, prepared_paths + begin, paint, context, runs);
    } else {
      transformed_mask::AppendGlyphRuns(
          span, transformed_mask::MakeBitmapOnlyFillPaint(paint), format, false,
          context, runs);
    }
    begin = end;
  }
}

void AppendDirectPaintPasses(const GlyphRunInput& input, const Paint& paint,
                             AtlasFormat format,
                             const GlyphRunBuildContext& context,
                             GlyphSubRunList& runs) {
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

  enum class Choice { kDirect, kFallback, kSkip };
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
        AppendFallbackGlyphRuns(span, pass.paint, format, context, runs,
                                paths.data() + begin);
      }
      begin = end;
    }
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
    case RasterMethod::kFallback:
      AppendFallbackGlyphRuns(input, paint, format, context, runs);
      break;
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
  std::vector<std::optional<GlyphFormat>> formats(input.count);
  for (uint32_t index = 0; index < input.count; ++index) {
    if (metrics[index] == nullptr) {
      continue;
    }
    formats[index] = metrics[index]->GetFormat();
    if (!formats[index]) {
      // An unspecified metric format does not imply a color glyph. Resolve
      // it from a fill image before choosing paint passes or a Path fallback.
      const GlyphData* image = nullptr;
      input.font.LoadGlyphBitmap(input.glyphs + index, 1, &image, metrics_paint,
                                 context.device_scale, Matrix{});
      if (image != nullptr) {
        switch (image->Image().format) {
          case BitmapFormat::kGray8:
            formats[index] = GlyphFormat::A8;
            break;
          case BitmapFormat::kRGBA8:
            formats[index] = GlyphFormat::RGBA32;
            break;
          case BitmapFormat::kBGRA8:
            formats[index] = GlyphFormat::BGRA32;
            break;
          default:
            break;
        }
      }
    }
  }
  auto atlas_format = [&](uint32_t index) {
    return FromGlyphFormat(*formats[index]);
  };

  uint32_t begin = 0;
  while (begin < input.count) {
    if (!formats[begin]) {
      ++begin;
      continue;
    }
    const AtlasFormat format = atlas_format(begin);
    uint32_t end = begin + 1;
    while (end < input.count && formats[end] && atlas_format(end) == format) {
      ++end;
    }
    AppendGlyphRuns(input.Slice(begin, end - begin), paint, format, context,
                    runs);
    begin = end;
  }
  return runs;
}

}  // namespace skity
