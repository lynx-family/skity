// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_COLOR_XFORM_HPP
#define SRC_GRAPHIC_COLOR_XFORM_HPP

#include <skity/graphic/color.hpp>
#include <skity/graphic/color_space.hpp>

namespace skity {

class Pixmap;

/**
 * A resolved src -> dst color space transform decomposed into folded stages:
 * decode (signal -> linear in the source space), a gamut matrix, and encode
 * (linear -> signal in the destination space). Every stage that cannot change
 * the result is folded away, and the whole transform collapses to a no-op
 * when the spaces match.
 *
 * This is the shared description of every color space boundary: CPU paths run
 * Apply() / TransformPixels() directly and the WGSL paths emit the same stages
 * as shader code, so all boundaries stay numerically consistent.
 */
struct ColorXformSteps {
  /// Stage 1: decode the source signal to linear; folded when src is linear.
  bool decode = false;
  float decode_fn[7] = {};
  /// Stage 2: move between linear gamuts; folded when the gamuts are
  /// bit-identical or the destination gamut is singular.
  bool gamut = false;
  /// Row-major precombined M_dst^-1 * M_src.
  float gamut_matrix[9] = {};
  /// Stage 3: encode linear to the destination signal; folded when dst is
  /// linear.
  bool encode = false;
  float encode_fn[7] = {};
  /// True when the destination transfer function is the sRGB curve, so WGSL
  /// emission can use the constant sRGB encode instead of the parametric
  /// form. CPU application always uses encode_fn.
  bool encode_is_srgb_curve = false;

  /// True when the transform cannot change any value.
  bool IsNoop() const { return !decode && !gamut && !encode; }

  /// Resolves the src -> dst transform, folding every stage that cannot
  /// change the result (the same folds ColorSpace::Convert applies). Null
  /// spaces mean the sRGB working space.
  static ColorXformSteps Resolve(const std::shared_ptr<const ColorSpace>& src,
                                 const std::shared_ptr<const ColorSpace>& dst);

  /// Applies the stages to an unpremultiplied color; alpha passes through and
  /// RGB is not clamped. Bit-identical to ColorSpace::Convert.
  Color4f Apply(Color4f color) const;
  void ApplyColors(Color4f colors[], int count) const;
};

/**
 * Converts a whole pixmap with the given steps. The transform itself runs on
 * unpremultiplied RGB, so premul <-> unpremul is handled on both sides; a
 * no-op transform never divides and re-multiplies, keeping same-layout copies
 * byte exact. Alpha-only (kA8) buffers are copied through; 16-bit formats are
 * rejected. Returns false when the layout is unsupported or the dimensions
 * differ.
 */
bool TransformPixels(const Pixmap& src, Pixmap* dst,
                     const ColorXformSteps& steps);

}  // namespace skity

#endif  // SRC_GRAPHIC_COLOR_XFORM_HPP
