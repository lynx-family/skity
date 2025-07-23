// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_render_pipeline_gl.hpp"

#include "src/gpu/gl/gpu_shader_function_gl.hpp"

namespace skity {

GLProgram::~GLProgram() {
  if (program_ != 0) {
    GL_CALL(DeleteProgram, program_);
  }
}

GLuint GLProgram::GetUniformLocation(const std::string name) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    return it->second;
  }
  GLint location = GL_CALL(GetUniformLocation, program_, name.c_str());
  uniform_locations_[name] = location;
  return location;
}

GLuint GLProgram::GetUniformBlockIndex(const std::string name) {
  auto it = uniform_block_indices_.find(name);
  if (it != uniform_block_indices_.end()) {
    return it->second;
  }
  GLint index = GL_CALL(GetUniformBlockIndex, program_, name.c_str());
  uniform_block_indices_[name] = index;
  return index;
}

GPURenderPipelineGL::GPURenderPipelineGL(
    const GPURenderPipelineDescriptor& desc)
    : GPURenderPipeline(desc) {
  GLuint program = GL_CALL(CreateProgram);
  GLint success;
  auto vs = static_cast<GPUShaderFunctionGL*>(desc.vertex_function.get())
                ->GetShader();
  auto fs = static_cast<GPUShaderFunctionGL*>(desc.fragment_function.get())
                ->GetShader();

  GL_CALL(AttachShader, program, vs);
  GL_CALL(AttachShader, program, fs);
  GL_CALL(LinkProgram, program);
  GL_CALL(GetProgramiv, program, GL_LINK_STATUS, &success);

  if (!success) {
    GLchar info_log[1024];
    GL_CALL(GetProgramInfoLog, program, 1024, nullptr, info_log);
    LOGE("OpenGL program link error : {}", info_log);
    GL_CALL(DeleteProgram, program);
    if (desc.error_callback) {
      desc.error_callback(info_log);
    }
    program = 0;
  }

  bool is_gles =
      static_cast<GPUShaderFunctionGL*>(desc.vertex_function.get())->IsGLES();
  auto gl_version_major =
      static_cast<GPUShaderFunctionGL*>(desc.vertex_function.get())
          ->GetGLVersionMajor();
  auto gl_version_minor =
      static_cast<GPUShaderFunctionGL*>(desc.vertex_function.get())
          ->GetGLVersionMinor();

  bool ubo_slot_in_shader = false;

  // In OpenGL 4.2 the shader can specify the binding point for the uniform
  // buffer in shader
  // In OpenGL 3.1 the shader can specify the binding point for the
  // uniform buffer in shader
  if (is_gles) {
    ubo_slot_in_shader = gl_version_major == 3 && gl_version_minor >= 1;
  } else {
    ubo_slot_in_shader = gl_version_major == 4 && gl_version_minor >= 2;
  }

  program_ = std::make_shared<GLProgram>(program, ubo_slot_in_shader);
}

GPURenderPipelineGL::GPURenderPipelineGL(
    std::shared_ptr<GLProgram> program,
    const skity::GPURenderPipelineDescriptor& desc)
    : GPURenderPipeline(desc), program_(std::move(program)) {}

}  // namespace skity
