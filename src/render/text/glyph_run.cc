// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/glyph_run.hpp"

#include <algorithm>
#include <cmath>

#include "src/render/text/atlas/atlas_manager.hpp"
#include "src/render/text/direct_glyph_run.hpp"
#include "src/render/text/glyph_run_fallback.hpp"
#include "src/render/text/sdf_glyph_run.hpp"
#include "src/render/text/text_render_control.hpp"
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

  if (context.options.direct && format == AtlasFormat::RGBA32 &&
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

  TextRenderControl control{!context.options.sdf};
  if (context.options.direct &&
      control.CanUseDirect(text_size, transform, paint,
                           input.font.GetTypeface())) {
    return RasterMethod::kDirect;
  }
  if (format == AtlasFormat::A8 &&
      control.CanUseSDF(text_size, paint, input.font.GetTypeface())) {
    return RasterMethod::kSDF;
  }
  return RasterMethod::kFallback;
}

void AppendGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                     AtlasFormat format, const GlyphRunBuildContext& context,
                     GlyphSubRunList& runs) {
  switch (SelectRasterMethod(input, paint, format, context)) {
    case RasterMethod::kDirect:
      AppendDirectGlyphRuns(input, paint, format, context, runs,
                            AppendFallbackGlyphRuns);
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
      !std::isfinite(context.device_scale) || context.device_scale <= 0.f ||
      !std::isfinite(input.font.GetSize()) || input.font.GetSize() <= 0.f) {
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
