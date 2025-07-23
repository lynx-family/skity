// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_EFFECT_PATH_EFFECT_HPP
#define INCLUDE_SKITY_EFFECT_PATH_EFFECT_HPP

#include <cstdint>
#include <memory>
#include <skity/macros.hpp>

namespace skity {

class Path;
class Paint;

/**
 * @class PathEffect
 *  PathEffect is the base class for objects in the SkPaint that affect the
 *  geometry of a drawing primitive before it is transformed by the canvas'
 *  matrix and drawn.
 *
 *  Dashing is implemented as a subclass of SkPathEffect.
 */
class SKITY_API PathEffect {
 public:
  virtual ~PathEffect() = default;
  PathEffect(const PathEffect&) = delete;
  PathEffect& operator=(const PathEffect&) = delete;

  /**
   * Given a src path(input), apply this effect to src path, returning the new
   * path in dst, and return true. If this effect cannot be applied, return
   * false.
   *
   * @param dst     output of this effect
   * @param src     input of this effect
   * @param stroke  specify if path need stroke
   * @param paint       current paint for drawing this path
   * @return true   this effect can be applied
   * @return false  this effect cannot be applied
   */
  bool FilterPath(Path* dst, Path const& src, bool stroke,
                  Paint const& paint) const;

  /**
   * If the PathEffect can be represented as a dash pattern, asADash will return
   * kDash and None otherwise. If a non NULL info is passed in, the various
   * DashInfo will be filled in if the PathEffect can be a dash pattern.
   *
   */
  enum class DashType {
    // ignores the info parameter
    kNone,
    // fills in all of the info parameter
    kDash,
  };

  struct DashInfo {
    DashInfo() : intervals(nullptr), count(0), phase(0) {}
    DashInfo(float* inv, int32_t c, float p)
        : intervals(inv), count(c), phase(p) {}

    // length of on/off intervals for dashed lines
    float* intervals;
    // number of intervals in the dash. should be even number
    int32_t count;
    // offset into the dashed interval pattern
    // mode the sum of all intervals
    float phase;
  };

  DashType AsADash(DashInfo* info) const;

  /**
   * Create DiscretePathEffect.
   *  This path effect chops a path into discrete segments, and randomly
   *  displaces them.
   */
  static std::shared_ptr<PathEffect> MakeDiscretePathEffect(
      float seg_length, float dev, uint32_t seed_assist = 0);

  static std::shared_ptr<PathEffect> MakeDashPathEffect(const float intervals[],
                                                        int count, float phase);

 protected:
  PathEffect() = default;

  virtual bool OnFilterPath(Path*, Path const&, bool, Paint const&) const = 0;

  virtual DashType OnAsADash(DashInfo*) const { return DashType::kNone; }
};

}  // namespace skity

#endif  // INCLUDE_SKITY_EFFECT_PATH_EFFECT_HPP
