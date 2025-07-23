// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_SHADER_FUNCTION_GL_HPP
#define SRC_GPU_GL_GPU_SHADER_FUNCTION_GL_HPP

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gpu_shader_function.hpp"

namespace skity {

class GPUShaderFunctionGL : public GPUShaderFunction {
 public:
  GPUShaderFunctionGL(std::string label, GPUShaderStage stage,
                      const char* source,
                      const std::vector<int32_t>& constant_values,
                      GPUShaderFunctionErrorCallback error_callback);

  ~GPUShaderFunctionGL() override;

  GLuint GetShader() const { return shader_; }

  bool IsValid() const override { return shader_ != 0; }

  void SetupGLVersion(uint32_t major, uint32_t minor, bool is_gles);

  uint32_t GetGLVersionMajor() const { return gl_version_major_; }

  uint32_t GetGLVersionMinor() const { return gl_version_minor_; }

  bool IsGLES() const { return is_gles_; }

 private:
  GLuint shader_;
  uint32_t gl_version_major_ = 3;
  uint32_t gl_version_minor_ = 3;
  bool is_gles_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_SHADER_FUNCTION_GL_HPP
