// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_LAYER_HPP
#define SRC_RENDER_HW_HW_LAYER_HPP

#include <memory>
#include <optional>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>

#include "skity/graphic/image.hpp"
#include "src/gpu/gpu_command_buffer.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/render/canvas_state.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_draw_pass.hpp"
#include "src/render/hw/hw_layer_state.hpp"

namespace skity {

class GPUDevice;
class GPUContext;

enum class LayerRTOrigin {
  kTopLeft,
  kBottomLeft,
};

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

  void Draw(GPURenderPass* render_pass, GPUCommandBuffer* cmd) override;

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
    auto pass = arena_allocator_->Make<HWDrawPass>();
    draw_passes_.push_back(pass);
  }

  ArenaAllocator* GetArenaAllocator() const { return arena_allocator_; }

  void SetRTOrigin(LayerRTOrigin origin) { rt_origin_ = origin; }

  virtual bool SupportsTextureCopyDstRead() const { return false; }

 protected:
  HWDrawState OnPrepare(skity::HWDrawContext* context) override;

  void OnGenerateCommand(HWDrawContext* context, HWDrawState state) override;

  virtual std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer* cmd, bool force_load) = 0;

  virtual bool OnCopyToDstTexture(GPUCommandBuffer* cmd,
                                  std::shared_ptr<GPUTexture> dst_texture,
                                  GPURegion copy_region) const {
    (void)cmd;
    (void)dst_texture;
    (void)copy_region;
    return false;
  }

  virtual void OnPostDraw(GPURenderPass* render_pass,
                          GPUCommandBuffer* cmd) = 0;

  virtual std::shared_ptr<GPUTexture> GetResolveColorTexture() const {
    return nullptr;
  }

 protected:
  HWDrawState GetLayerDrawState() const { return layer_state_; }

  GPUViewport GetViewport() const {
    return GPUViewport{
        0, 0, static_cast<float>(GetWidth()), static_cast<float>(GetHeight()),
        0, 1};
  }

  std::shared_ptr<Shader> CreateDrawLayerShader(
      GPUContext* gpu_context, std::shared_ptr<GPUTexture> texture,
      const Rect& bounds) const;

  std::shared_ptr<Shader> CreateDrawLayerShader(std::shared_ptr<Image> image,
                                                const Rect& bounds) const;

  /**
   * Generates a draw call that emulates load action for an MSAA target by
   * re-rendering the resolve texture back into the MSAA attachment.
   */
  std::optional<EmulatedLoadInfo> CreateEmulatedLoadInfo();

 private:
  void FlushPendingClip();

  bool TryMerge(HWDraw* draw);

  void PrepareReplayDraws(HWDrawPass* pass, HWDrawContext* context);

  std::optional<DstTextureCopyInfo> BuildDstTextureCopyInfo(
      const Rect& layer_space_bounds) const;

  Vec4 BuildDstUVMapping(const Rect& copy_rect) const;

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
  std::vector<HWDrawPass*> draw_passes_ = {};
  std::vector<HWDraw*> pending_clip_ = {};
  GPUDevice* gpu_device_ = {};
  Matrix bounds_to_physical_matrix_ = {};
  bool enable_merging_draw_call_ = {};
  ArenaAllocator* arena_allocator_ = nullptr;
  Vec2 scale_ = {1.f, 1.f};
  LayerRTOrigin rt_origin_ = LayerRTOrigin::kTopLeft;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_LAYER_HPP
