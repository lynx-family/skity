// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_WGX_PROGRAMMABLE_BLENDING_HPP
#define SRC_RENDER_HW_DRAW_WGX_PROGRAMMABLE_BLENDING_HPP

#include <memory>
#include <skity/graphic/blend_mode.hpp>
#include <string>

namespace skity {

enum class DstReadStrategy {
  kNonRequired,
  kFramebufferFetch,
  kTextureCopy,
};

/**
 * Common code generator for all advanced blending shader.
 */
class WGXProgrammableBlending {
 public:
  explicit WGXProgrammableBlending(BlendMode blend_mode,
                                   DstReadStrategy dst_read_strategy)
      : blend_mode_(blend_mode), dst_read_strategy_(dst_read_strategy) {}

  ~WGXProgrammableBlending() = default;

  std::string GenSourceWGSL() const;

  uint32_t GetProgrammableBlendingKey() const {
    return static_cast<uint32_t>(blend_mode_) |
           static_cast<uint32_t>(dst_read_strategy_) << 8;
  }

  bool SupportsFramebufferFetch() const {
    return dst_read_strategy_ == DstReadStrategy::kFramebufferFetch;
  }

  DstReadStrategy GetReadDstStrategy() const { return dst_read_strategy_; }

  static std::unique_ptr<WGXProgrammableBlending> Make(
      BlendMode blend_mode, DstReadStrategy dst_read_strategy) {
    return std::make_unique<WGXProgrammableBlending>(blend_mode,
                                                     dst_read_strategy);
  }

 private:
  BlendMode blend_mode_;
  DstReadStrategy dst_read_strategy_ = DstReadStrategy::kNonRequired;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_WGX_PROGRAMMABLE_BLENDING_HPP
