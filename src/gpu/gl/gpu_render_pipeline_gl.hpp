// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_RENDER_PIPELINE_GL_HPP
#define SRC_GPU_GL_GPU_RENDER_PIPELINE_GL_HPP

#include <string>
#include <unordered_map>

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"

namespace skity {

class GLProgram {
 public:
  explicit GLProgram(GLuint program, bool ubo_slot_in_shader)
      : program_(program), support_ubo_slot_in_shader_(ubo_slot_in_shader) {}

  ~GLProgram();

  GLuint GetProgram() const { return program_; }

  GLuint GetUniformLocation(const std::string name);

  GLuint GetUniformBlockIndex(const std::string name);

  bool SupportUBOSlotInShader() const { return support_ubo_slot_in_shader_; }

 private:
  std::unordered_map<std::string, GLuint> uniform_block_indices_;
  std::unordered_map<std::string, GLint> uniform_locations_;
  GLuint program_;
  bool support_ubo_slot_in_shader_ = false;
};

class GPURenderPipelineGL : public GPURenderPipeline {
 public:
  explicit GPURenderPipelineGL(const GPURenderPipelineDescriptor& desc);

  GPURenderPipelineGL(std::shared_ptr<GLProgram> program,
                      const GPURenderPipelineDescriptor& desc);

  ~GPURenderPipelineGL() override = default;

  GLuint GetProgramId() const { return program_->GetProgram(); }

  std::shared_ptr<GLProgram> GetProgram() const { return program_; }

  bool IsValid() const override {
    return GPURenderPipeline::IsValid() && program_->GetProgram() != 0;
  }

  bool SupportUBOSlotInShader() const {
    if (program_ == nullptr) {
      return false;
    }

    return program_->SupportUBOSlotInShader();
  }

 private:
  std::shared_ptr<GLProgram> program_;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_RENDER_PIPELINE_GL_HPP
