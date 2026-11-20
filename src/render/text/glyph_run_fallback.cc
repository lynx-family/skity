// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/glyph_run_fallback.hpp"

#include "src/render/text/drawable_glyph_run.hpp"
#include "src/render/text/path_glyph_run.hpp"
#include "src/render/text/transformed_mask_glyph_run.hpp"

namespace skity {

void AppendFallbackGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                             AtlasFormat format,
                             const GlyphRunBuildContext& context,
                             GlyphSubRunList& runs,
                             const GlyphData* const* prepared_paths,
                             bool retry_image) {
  if (format == AtlasFormat::RGBA32) {
    // Keep color content intact. Only consecutive glyphs with the same
    // representation are grouped, so overlapping mixed runs preserve order.
    auto drawables =
        context.options.drawable
            ? drawable::PrepareGlyphs(input, paint)
            : std::vector<std::shared_ptr<const GlyphDrawable>>(input.count);
    Paint fill = paint;
    fill.SetStyle(Paint::kFill_Style);
    uint32_t begin = 0;
    while (begin < input.count) {
      const bool supported = drawables[begin] != nullptr;
      uint32_t end = begin + 1;
      while (end < input.count && (drawables[end] != nullptr) == supported) {
        ++end;
      }
      auto span = input.Slice(begin, end - begin);
      if (supported) {
        drawable::AppendGlyphRuns(span, drawables.data() + begin, fill, context,
                                  runs);
      } else if (retry_image && context.options.transformed_mask) {
        transformed_mask::AppendGlyphRuns(span, fill, format, false, context,
                                          runs);
      }
      begin = end;
    }
    return;
  }

  std::vector<const GlyphData*> paths;
  if (!prepared_paths && context.options.path) {
    paths.resize(input.count);
    input.font.LoadGlyphPath(input.glyphs, input.count, paths.data());
    prepared_paths = paths.data();
  }
  auto has_outline = [&](uint32_t index) {
    return context.options.path && prepared_paths && prepared_paths[index] &&
           !prepared_paths[index]->GetPath().IsEmpty();
  };
  uint32_t begin = 0;
  while (begin < input.count) {
    const bool outline = has_outline(begin);
    uint32_t end = begin + 1;
    while (end < input.count && has_outline(end) == outline) {
      ++end;
    }
    const auto span = input.Slice(begin, end - begin);
    if (outline) {
      AppendPathGlyphRuns(span, prepared_paths + begin, paint, context, runs);
    } else if (context.options.transformed_mask) {
      transformed_mask::AppendGlyphRuns(
          span, transformed_mask::MakeBitmapOnlyFillPaint(paint), format, false,
          context, runs);
    }
    begin = end;
  }
}

}  // namespace skity
