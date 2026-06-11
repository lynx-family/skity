// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_RENDER_PRECOMPILE_CONTEXT_HPP
#define INCLUDE_SKITY_RENDER_PRECOMPILE_CONTEXT_HPP

#include <cstdint>
#include <memory>
#include <skity/macros.hpp>

namespace skity {

class GPUContext;
class Paint;
class PrecompileContextImpl;

enum class PrecompileColorType : uint8_t {
  kRGBA,
  kBGRA,
};

enum class PrecompileDrawType : uint16_t {
  kDrawRect,
  kDrawRRect,
  kDrawPath,
  kDrawText,
  kDrawSDFText,
  kDrawEmojiText,
  kDrawImage,
  kDrawImageRRect,
  kClipPath,
};

/**
 * Context used to warm up GPU pipeline variants before drawing. The context
 * should be created with values matching the surface that will later render
 * with the precompiled shaders.
 */
class SKITY_API PrecompileContext {
 public:
  ~PrecompileContext();

  /**
   * Precompile a set of common graphics, image, and text pipelines. The exact
   * pipeline set is an implementation detail and may evolve over time.
   */
  void PrecompileDefaultShaders() const;

  /**
   * Precompile pipelines for a specific draw type and paint combination.
   */
  void PrecompileDraw(PrecompileDrawType draw_type, const Paint& paint) const;

 private:
  friend class GPUContext;

  PrecompileContext(GPUContext* gpu_context, PrecompileColorType color_type,
                    bool enable_msaa);

  std::unique_ptr<PrecompileContextImpl> impl_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_RENDER_PRECOMPILE_CONTEXT_HPP
