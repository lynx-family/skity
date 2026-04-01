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
    GPULabel label, GPUShaderStage stage, const char* source,
    GPUShaderFunctionErrorCallback error_callback)
    : GPUShaderFunction(std::move(label)) {
  GLenum type = GetShaderType(stage);
  GLuint shader = GL_CALL(CreateShader, type);
  if (shader == 0) {
    LOGE("OpenGL CreateShader failed.");
    if (error_callback) {
      error_callback("OpenGL CreateShader failed.");
    }
    shader_ = shader;
    return;
  }
  GL_CALL(ShaderSource, shader, 1, &source, nullptr);
  GL_CALL(CompileShader, shader);

  GLint success = 0;
  GL_CALL(GetShaderiv, shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLint log_length = 0;
    GL_CALL(GetShaderiv, shader, GL_INFO_LOG_LENGTH, &log_length);
    std::string info_log_str;
    if (log_length > 0) {
      GLsizei max_length = std::min(log_length + 1, 4096);
      std::vector<GLchar> log_buffer(max_length, 0);
      GL_CALL(GetShaderInfoLog, shader, max_length, nullptr, log_buffer.data());
      info_log_str = log_buffer.data();
    }
    const char* info_log = !info_log_str.empty()
                               ? info_log_str.c_str()
                               : "Unknown shader compile error.";

    LOGE("OpenGL shader {} compile error : {}", GetLabel(), info_log);
#ifdef SKITY_LOG
    auto version = GL_CALL(GetString, GL_VERSION);
    auto vendor = GL_CALL(GetString, GL_VENDOR);
    auto renderer = GL_CALL(GetString, GL_RENDERER);
    LOGE("OpenGL version:{}  vendor:{}  renderer:{}",
         version ? reinterpret_cast<const char*>(version) : "N/A",
         vendor ? reinterpret_cast<const char*>(vendor) : "N/A",
         renderer ? reinterpret_cast<const char*>(renderer) : "N/A");
#endif  // SKITY_LOG
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
