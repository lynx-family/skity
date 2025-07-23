
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_BUFFER_GL_HPP
#define SRC_GPU_GL_GPU_BUFFER_GL_HPP

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gpu_buffer.hpp"

namespace skity {

class GPUBufferGL : public GPUBuffer {
 public:
  explicit GPUBufferGL(GPUBufferUsageMask usage);

  ~GPUBufferGL() override;

  void UploadData(void* data, size_t size) override;

  GLuint GetBufferId() const { return gl_buffer_; }

 private:
  GLenum target_;
  GLuint gl_buffer_;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_BUFFER_GL_HPP
