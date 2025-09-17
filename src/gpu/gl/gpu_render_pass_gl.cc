// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_render_pass_gl.hpp"

#include "src/gpu/gl/formats_gl.h"
#include "src/gpu/gl/gpu_buffer_gl.hpp"
#include "src/gpu/gl/gpu_render_pipeline_gl.hpp"
#include "src/gpu/gl/gpu_sampler_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/tracing.hpp"

namespace skity {

void GPURenderPassGL::EncodeCommands(std::optional<GPUViewport> viewport,
                                     std::optional<GPUScissorRect> scissor) {
  SKITY_TRACE_EVENT(GPURenderPassGL_EncodeCommands);
  GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, target_fbo_);

  auto target_width = GetDescriptor().GetTargetWidth();
  auto target_height = GetDescriptor().GetTargetHeight();

  GPUViewport v = viewport.value_or(GPUViewport{
      0,
      0,
      static_cast<float>(target_width),
      static_cast<float>(target_height),
      0,
      1,
  });
  GPUScissorRect s = scissor.value_or(GPUScissorRect{
      0,
      0,
      target_width,
      target_height,
  });

  GL_CALL(Viewport, v.x,                   // x
          target_height - v.y - v.height,  // y
          v.width,                         // width
          v.height);                       // height

  SetScissorBox(s.x, target_height - s.y - s.height, s.width, s.height);

  // sync state to default
  GL_CALL(Disable, GL_STENCIL_TEST);
  GL_CALL(StencilFunc, GL_ALWAYS, 0, 0xFF);
  GL_CALL(StencilOp, GL_KEEP, GL_KEEP, GL_KEEP);
  GL_CALL(StencilMask, 0xFF);
  GL_CALL(ColorMask, 1, 1, 1, 1);
  GL_CALL(Enable, GL_BLEND);
  GL_CALL(BlendFunc, GL_ZERO, GL_ZERO);

  SetDepthState(true, true, GPUCompareFunction::kAlways);

  Clear();
  if (after_cleanup_action_) {
    after_cleanup_action_();
  }

  for (auto command : GetCommands()) {
    SKITY_TRACE_EVENT(GPURenderPassGL_Drawcommand);
    if (!command->IsValid()) {
      continue;
    }

    // set scissor
    SetScissorBox(
        command->scissor_rect.x,
        target_height - command->scissor_rect.y - command->scissor_rect.height,
        command->scissor_rect.width, command->scissor_rect.height);

    auto pipeline = static_cast<skity::GPURenderPipelineGL*>(command->pipeline);
    // Set blending
    SetBlendFunc(
        ToBlendFactor(pipeline->GetDescriptor().target.src_blend_factor),
        ToBlendFactor(pipeline->GetDescriptor().target.dst_blend_factor));

    // color mask
    SetColorWriteMask(pipeline->GetDescriptor().target.write_mask != 0);

    auto const& depth_stencil = pipeline->GetDescriptor().depth_stencil;

    SetStencilState(depth_stencil.enable_stencil, depth_stencil.stencil_state,
                    command->stencil_reference);

    SetDepthState(depth_stencil.enable_depth,
                  depth_stencil.depth_state.enableWrite,
                  depth_stencil.depth_state.compare);

    // Set program
    GL_CALL(UseProgram, pipeline->GetProgramId());

    BindBuffer(GL_ARRAY_BUFFER,
               static_cast<GPUBufferGL*>(command->vertex_buffer.buffer)
                   ->GetBufferId());

    // Set vertex
    for (auto& buffer_layout : pipeline->GetDescriptor().buffers) {
      if (buffer_layout.step_mode == GPUVertexStepMode::kVertex) {
        BindBuffer(GL_ARRAY_BUFFER,
                   static_cast<GPUBufferGL*>(command->vertex_buffer.buffer)
                       ->GetBufferId());

        for (auto& attribute : buffer_layout.attributes) {
          GL_CALL(EnableVertexAttribArray, attribute.shader_location);
          GL_CALL(VertexAttribPointer, attribute.shader_location,
                  static_cast<uint32_t>(attribute.format), GL_FLOAT, GL_FALSE,
                  buffer_layout.array_stride,
                  reinterpret_cast<void*>(command->vertex_buffer.offset +
                                          attribute.offset));
          GL_CALL(VertexAttribDivisor, attribute.shader_location, 0);
        }
      } else {
        BindBuffer(GL_ARRAY_BUFFER,
                   static_cast<GPUBufferGL*>(command->instance_buffer.buffer)
                       ->GetBufferId());
        for (auto& attribute : buffer_layout.attributes) {
          GL_CALL(EnableVertexAttribArray, attribute.shader_location);
          GL_CALL(VertexAttribPointer, attribute.shader_location,
                  static_cast<uint32_t>(attribute.format), GL_FLOAT, GL_FALSE,
                  buffer_layout.array_stride,
                  reinterpret_cast<void*>(command->instance_buffer.offset +
                                          attribute.offset));
          GL_CALL(VertexAttribDivisor, attribute.shader_location, 1);
        }
      }
    }

    // Bind uniforms
    for (auto& binding : command->uniform_bindings) {
      if (!pipeline->SupportUBOSlotInShader()) {
        GL_CALL(UniformBlockBinding, pipeline->GetProgramId(),
                pipeline->GetProgram()->GetUniformBlockIndex(binding.name),
                binding.index);
      }

      BindBuffer(
          GL_UNIFORM_BUFFER,
          static_cast<GPUBufferGL*>(binding.buffer.buffer)->GetBufferId());

      GL_CALL(BindBufferRange, GL_UNIFORM_BUFFER, binding.index,
              static_cast<GPUBufferGL*>(binding.buffer.buffer)->GetBufferId(),
              binding.buffer.offset, binding.buffer.range);
    }

    // Bind textures
    uint32_t texture_index = 0;
    for (auto& binding : command->texture_sampler_bindings) {
      auto texture = static_cast<GPUTextureGL*>(binding.texture.get());
      auto sampler = static_cast<GPUSamplerGL*>(binding.sampler.get());

      GL_CALL(ActiveTexture, GL_TEXTURE0 + texture_index);
      texture->Bind();

      GL_CALL(Uniform1i,
              pipeline->GetProgram()->GetUniformLocation(binding.name),
              texture_index);
      texture->CombineSampler(sampler);

      texture_index++;
    }

    // Bind textures and samplers separately
    for (auto& binding : command->texture_bindings) {
      auto texture = static_cast<GPUTextureGL*>(binding.texture.get());

      GL_CALL(ActiveTexture, GL_TEXTURE0 + binding.index);
      texture->Bind();

      GL_CALL(Uniform1i,
              pipeline->GetProgram()->GetUniformLocation(binding.name),
              binding.index);
    }

    for (auto& binding : command->sampler_bindings) {
      auto sampler = static_cast<GPUSamplerGL*>(binding.sampler.get());

      if (binding.uints) {
        const auto& units = *binding.uints;

        for (auto unit : units) {
          GL_CALL(BindSampler, unit, sampler->GetSamplerID());
        }
      } else {
        GL_CALL(BindSampler, binding.index, sampler->GetSamplerID());
      }
    }

    BindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GPUBufferGL*>(command->index_buffer.buffer)->GetBufferId());

    // Draw elements
    if (command->IsInstanced()) {
      GL_CALL(DrawElementsInstanced, GL_TRIANGLES, command->index_count,
              GL_UNSIGNED_INT,
              reinterpret_cast<void*>(command->index_buffer.offset),
              command->instance_count);
    } else {
      GL_CALL(DrawElements, GL_TRIANGLES, command->index_count, GL_UNSIGNED_INT,
              reinterpret_cast<void*>(command->index_buffer.offset));
    }
  }

  SetScissorBox(s.x, target_height - s.y - s.height, s.width, s.height);

  if (GetDescriptor().stencil_attachment.texture &&
      GetDescriptor().stencil_attachment.store_op == GPUStoreOp::kDiscard &&
      target_fbo_ != 0) {
#if defined(SKITY_ANDROID)

    // glInvalidateFramebuffer is available above GLES 3.0
    // if GL_EXT_discard_framebuffer is not present
    // fallback to use glInvalidateFramebuffer
    if (GLInterface::GlobalInterface()->ext_discard_framebuffer) {
      GLenum discard_ap = GL_DEPTH_STENCIL_ATTACHMENT;
      GL_CALL(DiscardFramebufferEXT, GL_FRAMEBUFFER, 1, &discard_ap);
    } else {
      GLenum invalid_ap = GL_DEPTH_STENCIL_ATTACHMENT;
      GL_CALL(InvalidateFramebuffer, GL_FRAMEBUFFER, 1, &invalid_ap);
    }

#else
    // glInvalidateFramebuffer is available above GL 4.3
    // need to check if the function is present before use
    if (GLInterface::GlobalInterface()->fInvalidateFramebuffer != nullptr) {
      // FIXME: iOS need to seperate discard depth and stencil attachment
      // even if attached with GL_DEPTH_STENCIL_ATTACHMENT
      // and it worked fine in desktop and Android
      GLenum invalid_ap[2] = {GL_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT};
      GL_CALL(InvalidateFramebuffer, GL_FRAMEBUFFER, 2, invalid_ap);
    }

#endif
  }
  GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);
}

void GPURenderPassGL::Clear() {
  const auto& desc = GetDescriptor();

  GLuint clear_mask = 0;

  if (desc.color_attachment.load_op == GPULoadOp::kClear) {
    clear_mask |= GL_COLOR_BUFFER_BIT;

    GL_CALL(ClearColor, 0, 0, 0, 0);
  }

  if (desc.stencil_attachment.load_op == GPULoadOp::kClear) {
    clear_mask |= GL_STENCIL_BUFFER_BIT;

    GL_CALL(ClearStencil, desc.stencil_attachment.clear_value);
  }

  if (desc.depth_attachment.load_op == GPULoadOp::kClear) {
    clear_mask |= GL_DEPTH_BUFFER_BIT;

    GL_CALL(ClearDepthf, desc.depth_attachment.clear_value);
  }

  if (clear_mask) {
    GL_CALL(Clear, clear_mask);
  }
}

void GPURenderPassGL::SetScissorBox(uint32_t x, uint32_t y, uint32_t width,
                                    uint32_t height) {
  if (scissor_box_.x == x && scissor_box_.y == y &&
      scissor_box_.width == width && scissor_box_.height == height) {
    return;
  }

  GL_CALL(Scissor, x, y, width, height);

  scissor_box_.x = x;
  scissor_box_.y = y;
  scissor_box_.width = width;
  scissor_box_.height = height;
}

void GPURenderPassGL::SetColorWriteMask(bool enable) {
  if (enable == enable_color_write_) {
    return;
  }

  if (enable) {
    GL_CALL(ColorMask, 1, 1, 1, 1);
  } else {
    GL_CALL(ColorMask, 0, 0, 0, 0);
  }

  enable_color_write_ = enable;
}

void GPURenderPassGL::UseProgram(uint32_t program) {
  if (used_program_ == program) {
    return;
  }

  GL_CALL(UseProgram, program);

  used_program_ = program;
}

void GPURenderPassGL::SetStencilState(bool enable, const GPUStencilState& state,
                                      uint32_t ref) {
  if (enable == enable_stencil_test_ && state == stencil_state_ &&
      ref == stencil_reference_) {
    return;
  }

  if (enable) {
    GL_CALL(Enable, GL_STENCIL_TEST);
  } else {
    GL_CALL(Disable, GL_STENCIL_TEST);
  }

  // Set front stencil
  auto& front = state.front;
  GL_CALL(StencilFuncSeparate, GL_FRONT, ToCompareFunction(front.compare), ref,
          front.stencil_read_mask);
  GL_CALL(StencilOpSeparate, GL_FRONT, ToStencilOp(front.fail_op),
          ToStencilOp(front.depth_fail_op), ToStencilOp(front.pass_op));
  GL_CALL(StencilMaskSeparate, GL_FRONT, front.stencil_write_mask);

  // Set back stencil
  auto& back = state.back;
  GL_CALL(StencilFuncSeparate, GL_BACK, ToCompareFunction(back.compare), ref,
          back.stencil_read_mask);
  GL_CALL(StencilOpSeparate, GL_BACK, ToStencilOp(back.fail_op),
          ToStencilOp(back.depth_fail_op), ToStencilOp(back.pass_op));
  GL_CALL(StencilMaskSeparate, GL_BACK, back.stencil_write_mask);

  enable_stencil_test_ = enable;
  stencil_state_ = state;
  stencil_reference_ = ref;
}

void GPURenderPassGL::SetBlendFunc(uint32_t src, uint32_t dst) {
  bool disable = src == GL_ONE && dst == GL_ZERO;
  if (disable != disable_blend_) {
    if (disable) {
      GL_CALL(Disable, GL_BLEND);
    } else {
      GL_CALL(Enable, GL_BLEND);
    }
    disable_blend_ = disable;
  }

  if (disable_blend_) {
    return;
  }

  if (src == blend_src_ && dst == blend_dst_) {
    return;
  }

  GL_CALL(BlendFunc, src, dst);

  blend_src_ = src;
  blend_dst_ = dst;
}

void GPURenderPassGL::BindBuffer(uint32_t target, uint32_t buffer) {
  auto it = bound_buffer_.find(target);

  if (it != bound_buffer_.end() && it->second == buffer) {
    return;
  }

  GL_CALL(BindBuffer, target, buffer);

  bound_buffer_.insert_or_assign(target, buffer);
}

void GPURenderPassGL::SetDepthState(bool enable, bool writable,
                                    GPUCompareFunction func) {
  if (!enable) {
    GL_CALL(Disable, GL_DEPTH_TEST);
    return;
  }

  GL_CALL(Enable, GL_DEPTH_TEST);

  GL_CALL(DepthMask, writable);

  GL_CALL(DepthFunc, ToCompareFunction(func));
}

void GPURenderPassGL::BlitFramebuffer(uint32_t src_fbo, uint32_t dst_fbo,
                                      const Rect& src_rect,
                                      const Rect& dst_rect,
                                      uint32_t target_width,
                                      uint32_t target_height) {
  GL_CALL(BindFramebuffer, GL_READ_FRAMEBUFFER, src_fbo);
  GL_CALL(BindFramebuffer, GL_DRAW_FRAMEBUFFER, dst_fbo);
  GL_CALL(BlitFramebuffer, 0, 0, target_width, target_height, 0, 0,
          target_width, target_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

GLMSAAResolveRenderPass::GLMSAAResolveRenderPass(
    const skity::GPURenderPassDescriptor& desc, uint32_t target_fbo,
    uint32_t resolve_fbo)
    : GPURenderPassGL(desc, target_fbo), resolve_fbo_(resolve_fbo) {}

void GLMSAAResolveRenderPass::EncodeCommands(
    std::optional<GPUViewport> viewport,
    std::optional<GPUScissorRect> scissor) {
  GPURenderPassGL::EncodeCommands(viewport, scissor);

  auto target_width = GetDescriptor().GetTargetWidth();
  auto target_height = GetDescriptor().GetTargetHeight();
  auto src_rect = Rect::MakeLTRB(0, 0, target_width, target_height);
  auto dst_rect = src_rect;
  BlitFramebuffer(target_fbo_, resolve_fbo_, src_rect, dst_rect, target_width,
                  target_height);
  GL_CALL(BindFramebuffer, GL_FRAMEBUFFER, 0);
}

}  // namespace skity
