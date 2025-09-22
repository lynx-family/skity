// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_DRAW_HPP
#define SRC_RENDER_HW_HW_DRAW_HPP

#include <cstdint>
#include <memory>
#include <skity/geometry/matrix.hpp>
#include <utility>
#include <vector>

#include "src/render/hw/hw_render_target_cache.hpp"
#include "src/render/hw/hw_static_buffer.hpp"
#include "src/utils/arena_allocator.hpp"
#include "src/utils/vector_cache.hpp"

namespace skity {

class HWStageBuffer;
class GPURenderPass;
class HWPipelineLib;
class GPUContextImpl;

enum class HWDrawType {
  kUnknow,
  kPath,
  kText,
  kBlur,
  kStencil,
  kLayer,
  kClip,
};

struct HWDrawContext {
  float ctx_scale = 1.f;
  HWStageBuffer* stageBuffer = nullptr;
  HWPipelineLib* pipelineLib = nullptr;
  GPUContextImpl* gpuContext = nullptr;
  HWRenderTargetCache::Pool* pool = nullptr;
  Matrix mvp = {};
  VectorCache<float>* vertex_vector_cache;
  VectorCache<uint32_t>* index_vector_cache;
  uint32_t total_clip_depth = 0;
  ArenaAllocator* arena_allocator = nullptr;
  Vec2 scale = {1.f, 1.f};
  HWStaticBuffer* static_buffer = nullptr;
};

enum HWDrawState : uint32_t {
  kDrawStateNone = 0,
  kDrawStateStencil = 1 << 0,
  kDrawStateDepth = 1 << 1,
};

inline HWDrawState operator|(HWDrawState lhs, HWDrawState rhs) {
  return static_cast<HWDrawState>(static_cast<uint32_t>(lhs) |
                                  static_cast<uint32_t>(rhs));
}

inline HWDrawState operator&(HWDrawState lhs, HWDrawState rhs) {
  return static_cast<HWDrawState>(static_cast<uint32_t>(lhs) &
                                  static_cast<uint32_t>(rhs));
}

inline HWDrawState operator|=(HWDrawState& lhs, HWDrawState rhs) {
  lhs = static_cast<HWDrawState>(static_cast<uint32_t>(lhs) |
                                 static_cast<uint32_t>(rhs));
  return lhs;
}

class HWDraw {
 public:
  explicit HWDraw(Matrix transform) : transform_(transform) {}

  virtual ~HWDraw() = default;

  void SetSampleCount(uint32_t count) { sample_count_ = count; }

  uint32_t GetSampleCount() const { return sample_count_; }

  void SetColorFormat(GPUTextureFormat format) { target_format_ = format; }

  GPUTextureFormat GetColorFormat() const { return target_format_; }

  HWDrawState Prepare(HWDrawContext* context);

  void GenerateCommand(HWDrawContext* context, HWDrawState state);

  virtual void Draw(GPURenderPass* render_pass) = 0;

  const Matrix& GetTransform() const { return transform_; }

  bool IsAntiAlias() const { return anti_alias_; }

  void SetAntiAlias(bool aa) { anti_alias_ = aa; }

  void SetScissorBox(const Rect& rect) { scissor_rect_ = rect; }

  const Rect& GetScissorBox() const { return scissor_rect_; }

  void SetClipDraw(HWDraw* clip_draw) { clip_draw_ = clip_draw; }

  const HWDraw* GetClipDraw() const { return clip_draw_; }

  uint32_t GetClipDepth() const { return clip_depth_; }

  float GetClipValue() const { return clip_value_; }

  const Rect& GetLayerSpaceBounds() const { return layer_space_bounds_; }

  void SetLayerSpaceBounds(const Rect& layer_space_bounds) {
    layer_space_bounds_ = layer_space_bounds;
  }

  bool MergeIfPossible(HWDraw* draw) {
    if (GetDrawType() != draw->GetDrawType() ||
        GetDrawType() == HWDrawType::kUnknow) {
      return false;
    }

    if (GetTransform() != draw->GetTransform() ||
        GetClipDraw() != draw->GetClipDraw() ||
        GetScissorBox() != draw->GetScissorBox()) {
      return false;
    }

    bool merged = OnMergeIfPossible(draw);
    if (merged) {
      layer_space_bounds_.Join(draw->layer_space_bounds_);
    }
    return merged;
  }

  virtual HWDrawType GetDrawType() const { return HWDrawType::kUnknow; }

  void SetClipDepth(uint32_t clip_depth) { clip_depth_ = clip_depth; }

 protected:
  virtual HWDrawState OnPrepare(HWDrawContext* context) = 0;

  virtual void OnGenerateCommand(HWDrawContext* context, HWDrawState state) = 0;

  virtual bool OnMergeIfPossible(HWDraw* draw) { return false; }

  void SetTransform(const Matrix& matrix) { transform_ = matrix; }

 private:
  Matrix transform_;
  uint32_t clip_depth_;
  uint32_t sample_count_ = 1;
  float clip_value_ = 0.f;
  GPUTextureFormat target_format_ = GPUTextureFormat::kRGBA8Unorm;
  bool anti_alias_ = false;
  bool prepared_ = false;
  bool generated_ = false;
  HWDrawState draw_state_ = HWDrawState::kDrawStateNone;
  Rect scissor_rect_ = {};
  Rect layer_space_bounds_ = Rect::MakeLTRB(-1E9F, -1E9F, 1E9F, 1E9F);
  HWDraw* clip_draw_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_DRAW_HPP
