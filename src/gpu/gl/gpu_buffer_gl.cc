// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_buffer_gl.hpp"

#include "src/tracing.hpp"

namespace skity {

GPUBufferGL::GPUBufferGL(GPUBufferUsageMask usage) : GPUBuffer(usage) {
  GL_CALL(GenBuffers, 1, &gl_buffer_);

  if (usage & GPUBufferUsage::kIndexBuffer) {
    target_ = GL_ELEMENT_ARRAY_BUFFER;
  } else {
    target_ = GL_ARRAY_BUFFER;
  }
}

GPUBufferGL::~GPUBufferGL() { GL_CALL(DeleteBuffers, 1, &gl_buffer_); }

void GPUBufferGL::UploadData(void* data, size_t size) {
  SKITY_TRACE_EVENT(GPUBufferGL_UploadData);
  if (size == 0 || data == nullptr) {
    return;
  }

  GL_CALL(BindBuffer, target_, gl_buffer_);

  /**
   * according to filament, glBufferData is generally faster (or not worse) than
   * glBufferSubData.
   *
   * So we use glBufferData here. and hope this change can solve:
   * https://t.wtturl.cn/iPeVw7hC/
   * https://t.wtturl.cn/iPeq66wS/
   *
   * Since we use glBufferData, also change to use GL_STATIC_DRAW. since the
   * data only update once.
   */
  GL_CALL(BufferData, target_, size, data, GL_STATIC_DRAW);

  GL_CALL(BindBuffer, target_, 0);
}

}  // namespace skity
