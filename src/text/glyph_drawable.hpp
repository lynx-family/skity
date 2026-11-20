// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_GLYPH_DRAWABLE_HPP
#define SRC_TEXT_GLYPH_DRAWABLE_HPP

#include <memory>
#include <skity/recorder/display_list.hpp>
#include <utility>

namespace skity {

// A self-contained glyph drawing in strike coordinates. It owns all recorded
// resources and can outlive both the scaler cache entry and the typeface.
class GlyphDrawable final {
 public:
  static std::shared_ptr<const GlyphDrawable> Make(
      std::unique_ptr<DisplayList> commands, const Rect& bounds) {
    if (!commands || commands->Empty() || bounds.IsEmpty() ||
        !bounds.IsFinite()) {
      return nullptr;
    }
    // Cache path bounds and convexity before raster threads share this glyph.
    commands->PreparePathCaches();
    return std::shared_ptr<const GlyphDrawable>(
        new GlyphDrawable(std::move(commands), bounds));
  }

  const Rect& GetBounds() const { return bounds_; }
  void Draw(Canvas* canvas) const { commands_->Draw(canvas); }

 private:
  GlyphDrawable(std::unique_ptr<DisplayList> commands, const Rect& bounds)
      : commands_(std::move(commands)), bounds_(bounds) {}

  const std::unique_ptr<const DisplayList> commands_;
  const Rect bounds_;
};

}  // namespace skity

#endif  // SRC_TEXT_GLYPH_DRAWABLE_HPP
