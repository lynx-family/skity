// Copyright 2013 The Flutter Authors. All rights reserved.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_TILE_MODE_HPP
#define INCLUDE_SKITY_GRAPHIC_TILE_MODE_HPP

namespace skity {
/// An enum to define how to repeat, fold, or omit colors outside of the
/// typically defined range of the source of the colors (such as the
/// bounds of an image or the defining geometry of a gradient).
enum class TileMode {
  /// Replicate the edge color if the shader draws outside of its original
  /// bounds.
  kClamp,

  /// Repeat the shader's image horizontally and vertically (or both along and
  /// perpendicular to a gradient's geometry).
  kRepeat,

  /// Repeat the shader's image horizontally and vertically, seamlessly
  /// alternating mirrored images.
  kMirror,

  /// Render the shader's image pixels only within its original bounds. If the
  /// shader draws outside of its original bounds, transparent black is drawn
  /// instead.
  kDecal,
};
}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_TILE_MODE_HPP
