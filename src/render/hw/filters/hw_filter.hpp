// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_FILTERS_HW_FILTER_HPP
#define SRC_RENDER_HW_FILTERS_HW_FILTER_HPP

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <utility>
#include <vector>

#include "src/gpu/gpu_command_buffer.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/render/hw/hw_draw.hpp"

namespace skity {

struct HWFilterOutput {
  std::shared_ptr<GPUTexture> texture;
  Rect layer_bounds;
  Matrix matrix = Matrix{};
};

struct HWFilterContext {
  GPUDevice* device;
  GPUContextImpl* gpu_context;
  HWDrawContext* draw_context;
  HWFilterOutput source;
  std::shared_ptr<GPUCommandBuffer> command_buffer;
  Vec2 scale;
};

class AutoSetMVP {
 public:
  AutoSetMVP(HWDrawContext* draw_context, const Rect& layer_bounds)
      : draw_context_(draw_context), prev_mvp_(draw_context->mvp) {
    draw_context_->mvp = glm::ortho(layer_bounds.Left(), layer_bounds.Right(),
                                    layer_bounds.Bottom(), layer_bounds.Top());
  }

  ~AutoSetMVP() { draw_context_->mvp = prev_mvp_; }

 private:
  HWDrawContext* draw_context_;
  Matrix prev_mvp_;
};

class HWFilter {
 public:
  explicit HWFilter(std::vector<std::shared_ptr<HWFilter>> inputs)
      : inputs_(std::move(inputs)) {}

  HWFilterOutput Filter(HWFilterContext& context);

  virtual ~HWFilter() = default;

 protected:
  std::shared_ptr<GPUTexture> CreateOutputTexture(
      GPUTextureFormat format, Vec2 output_texture_size,
      const HWFilterContext& context);

  GPURenderPassDescriptor CreateRenderPassDesc(
      std::shared_ptr<GPUTexture> output_texture);

  void DrawChildrenOutputs(const HWFilterContext& context,
                           GPURenderPass* render_pass, Vec2 output_texture_size,
                           GPUTextureFormat color_format,
                           const Rect& layer_bounds,
                           const std::vector<HWFilterOutput>& children_outputs);

  HWFilterOutput GetChildOutput(size_t index, const HWFilterContext& context,
                                GPUCommandBuffer* command_buffer);

  size_t GetChildCount() const { return inputs_.size(); }

  virtual HWFilterOutput DoFilter(const HWFilterContext& context,
                                  GPUCommandBuffer* command_buffer) = 0;

 private:
  void InternalDrawChildrenOutpusWGX(
      const HWFilterContext& context, GPURenderPass* render_pass,
      Vec2 output_texture_size, GPUTextureFormat color_format,
      const Rect& layer_bounds,
      const std::vector<HWFilterOutput>& children_outputs);

 private:
  std::vector<std::shared_ptr<HWFilter>> inputs_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_FILTERS_HW_FILTER_HPP
