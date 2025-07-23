// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_RENDER_PASS_HPP
#define SRC_GPU_GPU_RENDER_PASS_HPP

#include <memory>
#include <optional>
#include <skity/geometry/rect.hpp>
#include <unordered_map>
#include <vector>

#include "src/gpu/gpu_buffer.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/utils/arena_allocator.hpp"
#include "src/utils/array_list.hpp"

namespace skity {

struct GPUColor {
  double r;
  double g;
  double b;
  double a;
};

enum class GPULoadOp {
  kDontCare,
  kLoad,
  kClear,
};

enum class GPUStoreOp {
  kStore,
  kDiscard,
};

struct GPUAttachment {
  std::shared_ptr<GPUTexture> texture;
  std::shared_ptr<GPUTexture> resolve_texture;
  GPULoadOp load_op = GPULoadOp::kDontCare;
  GPUStoreOp store_op = GPUStoreOp::kStore;
};

struct GPUColorAttachment : public GPUAttachment {
  GPUColor clear_value;
};

struct GPUStencilAttachment : public GPUAttachment {
  uint32_t clear_value = 0;
};

struct GPUDepthAttachment : public GPUAttachment {
  float clear_value = 0.f;
};

struct GPURenderPassDescriptor {
  GPUColorAttachment color_attachment;
  GPUStencilAttachment stencil_attachment;
  GPUDepthAttachment depth_attachment;

  uint32_t GetTargetWidth() const {
    if (!color_attachment.texture) {
      return 0;
    }
    return color_attachment.texture->GetDescriptor().width;
  }

  uint32_t GetTargetHeight() const {
    if (!color_attachment.texture) {
      return 0;
    }
    return color_attachment.texture->GetDescriptor().height;
  }
};

struct GPUViewport {
  float x;
  float y;
  float width;
  float height;
  float min_depth;
  float max_depth;
};

struct GPUScissorRect {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

enum class GPUIndexFormat { kUint16, kUint32 };

struct UniformBinding {
  GPUShaderStageMask stages;
  uint32_t index;
  std::string name;
  GPUBufferView buffer;
};

/**
 * Represent the `sampler2D` in GLSL shader. which is a combination of texture
 * and sampler.
 *
 * In Metal, we bind texture and sampler in same slot value as the index field
 * specified.
 * For example: If index is 9.
 * In Metal, we bind texture in `[[texture(9)]]` and sampler in `[[sampler(9)]]`
 */
struct TextureSamplerBinding {
  GPUShaderStageMask stages;
  uint32_t index;
  std::string name;
  std::shared_ptr<GPUTexture> texture;
  std::shared_ptr<GPUSampler> sampler;
};

/**
 * Represent the `texture_2d<f32>` in WGSL shader.
 * Only used when enable dynamic shader pipeline.
 */
struct TextureBinding {
  GPUShaderStageMask stages;
  /*
   * The index is generate by `wgsl-cross`.
   *
   * In OpenGL, this value is the `texture unit` index.
   *   For example: If index is 9. Means we bind the texture in `GL_TEXTURE9`
   *
   * In Metal, this value is the `texture slot` value.
   *   For example: If index is 9. Means we bind the texture in `[[texture(9)]]`
   */
  uint32_t index;
  std::string name;
  std::shared_ptr<GPUTexture> texture;
};

/**
 * Represent the `sampler` in WGSL shader.
 * Only used when enable dynamic shader pipeline.
 */
struct SamplerBinding {
  GPUShaderStageMask stages;
  /**
   * The index is generate by `wgsl-cross`.
   *
   * In Metal this value is the `sampler slot` value.
   *   For example: If index is 9. Means we bind the sampler in `[[sampler(9)]]`
   *
   * In OpenGL, this value is the `texture unit` index.
   *   For example: If index is 9. Means we bind the sampler in `GL_TEXTURE9`
   */
  uint32_t index;
  /**
   * This value is generate by `wgsl-cross`. And is only valid in OpenGL.
   * If the value is not `nullopt`, it means the sampler object is used by more
   * than one `texture_2d<f32>` in the orign WGSL source code.
   * for example:
   * ```
   *    var tex0 : texture_2d<f32>;
   *    var tex1 : texture_2d<f32>;
   *    var tex2 : texture_2d<f32>;
   *    var sampler0 : sampler;
   *
   *    textureSample(tex0, sampler0, tex0_coord);
   *    textureSample(tex1, sampler0, tex1_coord);
   *    textureSample(tex2, sampler0, tex2_coord);
   * ```
   */
  std::optional<std::vector<uint32_t>> uints = std::nullopt;
  std::string name;
  std::shared_ptr<GPUSampler> sampler;
};

struct Command {
  GPURenderPipeline* pipeline;
  GPUBufferView index_buffer;
  GPUBufferView vertex_buffer;
  ArrayList<UniformBinding, 4> uniform_bindings;
  ArrayList<TextureSamplerBinding, 4> texture_sampler_bindings;
  ArrayList<TextureBinding, 4> texture_bindings;
  ArrayList<SamplerBinding, 4> sampler_bindings;
  uint32_t stencil_reference = 0u;
  uint32_t index_count = 0u;

  GPUScissorRect scissor_rect = {};

  bool IsValid() const {
    if (pipeline == nullptr) {
      return false;
    }

    if (index_count == 0) {
      return false;
    }

    if (vertex_buffer.buffer == nullptr) {
      return false;
    }

    if (index_buffer.buffer == nullptr) {
      return false;
    }

    for (const auto& uniform : uniform_bindings) {
      if (uniform.buffer.buffer == nullptr) {
        return false;
      }
    }

    for (const auto& texture : texture_sampler_bindings) {
      if (texture.texture == nullptr) {
        return false;
      }
    }

    return true;
  }
};

class GPURenderPass {
 public:
  explicit GPURenderPass(const GPURenderPassDescriptor& desc) : desc_(desc) {}

  virtual ~GPURenderPass() = default;

  void AddCommand(Command* command) {
    if (!command->IsValid()) {
      return;
    }

    commands_.push_back(command);
  }

  const ArrayList<Command*, 32>& GetCommands() const { return commands_; }

  virtual void EncodeCommands(
      std::optional<GPUViewport> viewport = std::nullopt,
      std::optional<GPUScissorRect> scissor = std::nullopt) = 0;

  const GPURenderPassDescriptor& GetDescriptor() const { return desc_; }

  void SetArenaAllocator(ArenaAllocator* arena_allocator) {
    DEBUG_ASSERT(commands_.size() == 0);
    commands_.SetArenaAllocator(arena_allocator);
  }

 private:
  GPURenderPassDescriptor desc_;
  ArrayList<Command*, 32> commands_;
};

class GPURenderPassProxy : public GPURenderPass {
 public:
  explicit GPURenderPassProxy(const GPURenderPassDescriptor& desc)
      : GPURenderPass(desc) {}

  void EncodeCommands(
      std::optional<GPUViewport> viewport = std::nullopt,
      std::optional<GPUScissorRect> scissor = std::nullopt) override {
    viewport_ = viewport;
    scissor_ = scissor;
  }

 private:
  friend class GPUCommandBufferProxy;
  std::optional<GPUViewport> viewport_;
  std::optional<GPUScissorRect> scissor_;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_RENDER_PASS_HPP
