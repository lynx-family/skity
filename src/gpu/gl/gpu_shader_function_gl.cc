// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_shader_function_gl.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace skity {

inline static GLenum GetShaderType(GPUShaderStage shader_stage) {
  switch (shader_stage) {
    case GPUShaderStage::kVertex:
      return GL_VERTEX_SHADER;
    case GPUShaderStage::kFragment:
      return GL_FRAGMENT_SHADER;
  }
}

GPUShaderFunctionGL::~GPUShaderFunctionGL() {
  if (shader_ != 0) {
    GL_CALL(DeleteShader, shader_);
    shader_ = 0;
  }
}

GPUShaderFunctionGL::GPUShaderFunctionGL(
    std::string label, GPUShaderStage stage, const char* source,
    const std::vector<int32_t>& constant_values,
    GPUShaderFunctionErrorCallback error_callback)
    : GPUShaderFunction(std::move(label)) {
  GLenum type = GetShaderType(stage);
  GLuint shader = GL_CALL(CreateShader, type);
  if (constant_values.empty()) {
    GL_CALL(ShaderSource, shader, 1, &source, nullptr);
  } else {
    std::string source_str = source;
    auto index = source_str.find('\n');
    std::stringstream ss;
    int i = 0;
    for (int32_t value : constant_values) {
      ss << "#define SPIRV_CROSS_CONSTANT_ID_" << i << " " << value << '\n';
      i++;
    }
    source_str.insert(index + 1, ss.str());
    source = source_str.c_str();
    GL_CALL(ShaderSource, shader, 1, &source, nullptr);
  }
  GL_CALL(CompileShader, shader);
  GLint success;
  GL_CALL(GetShaderiv, shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLchar info_log[1024];
    GL_CALL(GetShaderInfoLog, shader, 1024, nullptr, info_log);
    LOGE("OpenGL shader compile error : {}", info_log);
    if (error_callback) {
      error_callback(info_log);
    }
    GL_CALL(DeleteShader, shader);
    shader_ = 0;
    return;
  }
  shader_ = shader;
}

void GPUShaderFunctionGL::SetupGLVersion(uint32_t major, uint32_t minor,
                                         bool is_gles) {
  gl_version_major_ = major;
  gl_version_minor_ = minor;
  is_gles_ = is_gles;
}

}  // namespace skity
