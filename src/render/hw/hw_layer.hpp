// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_LAYER_HPP
#define SRC_RENDER_HW_HW_LAYER_HPP

#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>

#include "src/gpu/gpu_command_buffer.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/canvas_state.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_layer_state.hpp"

namespace skity {

class GPUDevice;
class GPUContext;

class HWLayer : public HWDraw {
 public:
  /**
   *
   * @param matrix          self transform matrix when generate Command
   * @param depth           self depth in the total save stack
   * @param bounds          logical bounds in parent coordinate when generate
   *                        Command
   * @param width           physical width when generate backend texture or
   *                        framebuffer
   * @param height          physical height when generate backend texture or
   *                        framebuffer
   */
  HWLayer(Matrix matrix, int32_t depth, Rect bounds, uint32_t width,
          uint32_t height);

  ~HWLayer() override = default;

  void Draw(GPURenderPass* render_pass) override;

  HWLayerState* GetState();

  void AddDraw(HWDraw* draw);

  void AddClip(HWDraw* draw);

  void AddRectClip(const Rect& local_rect, const Matrix& matrix);

  void Restore();

  void RestoreToCount(int32_t count);

  uint32_t GetWidth() const { return width_; }

  uint32_t GetHeight() const { return height_; }

  const Rect& GetBounds() const { return bounds_; }

  const Matrix& GetWorldMatrix() const { return world_matrix_; }

  void SetWorldMatrix(const Matrix& matrix) { world_matrix_ = matrix; }

  void SetScale(Vec2 scale) { scale_ = scale; }

  Vec2 GetScale() const { return scale_; }

  Matrix GetLayerPhysicalMatrix(const Matrix& matrix) const;

  Rect CalculateLayerSpaceBounds(const Rect& local_rect,
                                 const Matrix& matrix) const;

  void SetEnableMergingDrawCall(bool enable) {
    enable_merging_draw_call_ = enable;
  }

  void SetArenaAllocator(ArenaAllocator* arena_allocator) {
    arena_allocator_ = arena_allocator;
  }

  ArenaAllocator* GetArenaAllocator() const { return arena_allocator_; }

 protected:
  HWDrawState OnPrepare(skity::HWDrawContext* context) override;

  void OnGenerateCommand(HWDrawContext* context, HWDrawState state) override;

  virtual std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer* cmd) = 0;

  virtual void OnPostDraw(GPURenderPass* render_pass,
                          GPUCommandBuffer* cmd) = 0;

  std::shared_ptr<GPUCommandBuffer> CreateCommandBuffer();

 protected:
  std::vector<HWDraw*>& GetDrawOps() { return draw_ops_; }

  HWDrawState GetLayerDrawState() const { return layer_state_; }

  GPUViewport GetViewport() const {
    return GPUViewport{
        0, 0, static_cast<float>(GetWidth()), static_cast<float>(GetHeight()),
        0, 1};
  }

  std::shared_ptr<Shader> CreateDrawLayerShader(
      GPUContext* gpu_context, std::shared_ptr<GPUTexture> texture,
      const Rect& bounds) const;

 private:
  void FlushPendingClip();

  bool TryMerge(HWDraw* draw);

 private:
  HWLayerState state_;
  // logical bounds used when layer rendered backend to parent
  Rect bounds_ = {};
  uint32_t width_ = {};
  uint32_t height_ = {};
  HWDrawState layer_state_ = HWDrawState::kDrawStateNone;
  /**
   * World matrix is the total matrix from root layer to parent layer
   * When saveLayer inside a layer, we need to use WordMatrix * CurrentMatrix()
   * to get the total transform and calculate the physical size of SubLayer
   */
  Matrix world_matrix_ = {};
  std::vector<HWDraw*> draw_ops_ = {};
  std::vector<HWDraw*> pending_clip_ = {};
  GPUDevice* gpu_device_ = {};
  Matrix bounds_to_physical_matrix_ = {};
  bool enable_merging_draw_call_ = {};
  ArenaAllocator* arena_allocator_ = nullptr;
  Vec2 scale_ = {1.f, 1.f};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_LAYER_HPP
