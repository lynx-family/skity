
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gl_interface.hpp"

#include <cstring>
#include <mutex>
#include <string_view>
#include <vector>

namespace skity {

struct GLExtensions {
  std::vector<std::string_view> extensions;

  bool Contains(std::string_view name) const {
    for (auto extension : extensions) {
      if (extension == name) {
        return true;
      }
    }
    return false;
  }
};

GLInterface *g_interface = nullptr;

#define GET_PROC(F) \
  g_interface->f##F = (decltype(g_interface->f##F))loader("gl" #F)

void GLInterface::InitGlobalInterface(void *proc_loader) {
  static std::mutex g_mutex = {};

  std::lock_guard<std::mutex> lock(g_mutex);

  if (g_interface) {
    return;
  }

  g_interface = new GLInterface;

  auto loader = reinterpret_cast<GLADloadfunc>(proc_loader);

  GET_PROC(ActiveTexture);
  GET_PROC(AttachShader);
  GET_PROC(BindAttribLocation);
  GET_PROC(BindBuffer);
  GET_PROC(BindBufferRange);
  GET_PROC(BindFramebuffer);
  GET_PROC(BindRenderbuffer);
  GET_PROC(BindSampler);
  GET_PROC(BindTexture);
  GET_PROC(BindVertexArray);
  GET_PROC(BlendColor);
  GET_PROC(BlendFunc);
  GET_PROC(BlendEquation);
  GET_PROC(BlitFramebuffer);
  GET_PROC(BufferData);
  GET_PROC(BufferSubData);
  GET_PROC(CheckFramebufferStatus);
  GET_PROC(Clear);
  GET_PROC(ClearBufferfi);
  GET_PROC(ClearBufferfv);
  GET_PROC(ClearColor);
  GET_PROC(ClearDepthf);
  GET_PROC(ClearStencil);
  GET_PROC(ColorMask);
  GET_PROC(CompileShader);
  GET_PROC(CopyTexSubImage2D);
  GET_PROC(ReadBuffer);
  GET_PROC(CreateProgram);
  GET_PROC(CreateShader);
  GET_PROC(CullFace);
  GET_PROC(DeleteBuffers);
  GET_PROC(DeleteFramebuffers);
  GET_PROC(DeleteProgram);
  GET_PROC(DeleteRenderbuffers);
  GET_PROC(DeleteShader);
  GET_PROC(DeleteTextures);
  GET_PROC(DeleteVertexArrays);
  GET_PROC(DepthMask);
  GET_PROC(DepthFunc);
  GET_PROC(Disable);
  GET_PROC(DisableVertexArrayAttrib);
  GET_PROC(DisableVertexAttribArray);
  GET_PROC(DrawArrays);
  GET_PROC(DrawArraysIndirect);
  GET_PROC(DrawArraysInstanced);
  GET_PROC(DrawBuffer);
  GET_PROC(DrawBuffers);
  GET_PROC(DrawElements);
  GET_PROC(Enable);
  GET_PROC(EnableVertexAttribArray);
  GET_PROC(Flush);
  GET_PROC(FramebufferRenderbuffer);
  GET_PROC(FramebufferTexture2D);
  GET_PROC(GenBuffers);
  GET_PROC(GenFramebuffers);
  GET_PROC(GenRenderbuffers);
  GET_PROC(GenTextures);
  GET_PROC(GenerateMipmap);
  GET_PROC(GenVertexArrays);
  GET_PROC(GetAttribLocation);
  GET_PROC(GetError);
  GET_PROC(GetIntegerv);
  GET_PROC(GetProgramInfoLog);
  GET_PROC(GetProgramiv);
  GET_PROC(GetShaderInfoLog);
  GET_PROC(GetShaderiv);
  GET_PROC(GetString);
  GET_PROC(GetStringi);
  GET_PROC(GetUniformLocation);
  GET_PROC(GetUniformBlockIndex);
  GET_PROC(InvalidateFramebuffer);
  GET_PROC(LinkProgram);
  GET_PROC(PixelStorei);
  GET_PROC(ReadPixels);
  GET_PROC(RenderbufferStorage);
  GET_PROC(RenderbufferStorageMultisample);
  GET_PROC(Scissor);
  GET_PROC(ShaderSource);
  GET_PROC(StencilFunc);
  GET_PROC(StencilFuncSeparate);
  GET_PROC(StencilMask);
  GET_PROC(StencilMaskSeparate);
  GET_PROC(StencilOp);
  GET_PROC(StencilOpSeparate);
  GET_PROC(TexImage2D);
  GET_PROC(TexImage2DMultisample);
  GET_PROC(TexParameteri);
  GET_PROC(TexSubImage2D);
  GET_PROC(UniformBlockBinding);
  GET_PROC(Uniform1f);
  GET_PROC(Uniform1fv);
  GET_PROC(Uniform1i);
  GET_PROC(Uniform1iv);
  GET_PROC(Uniform2f);
  GET_PROC(Uniform2fv);
  GET_PROC(Uniform2i);
  GET_PROC(Uniform3f);
  GET_PROC(Uniform3fv);
  GET_PROC(Uniform4f);
  GET_PROC(Uniform4fv);
  GET_PROC(Uniform4i);
  GET_PROC(Uniform4iv);
  GET_PROC(UniformMatrix4fv);
  GET_PROC(UseProgram);
  GET_PROC(VertexAttribPointer);
  GET_PROC(Viewport);
  GET_PROC(GenSamplers);
  GET_PROC(DeleteSamplers);
  GET_PROC(SamplerParameteri);
  GET_PROC(VertexAttribDivisor);
  GET_PROC(DrawElementsInstanced);

  g_interface->LoadExtensions(loader);
}

GLInterface *GLInterface::GlobalInterface() { return g_interface; }

bool GLInterface::CanUseMSAA() {
  return ext_multisampled_render_to_texture ||
         fRenderbufferStorageMultisample != nullptr;
}

bool GLInterface::LoadExtensions(GLADloadfunc loader) {
  GLExtensions extensions{};

  if (fGetStringi != nullptr && fGetIntegerv != nullptr) {
    int32_t num_extensions = 0;
    fGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

    extensions.extensions.resize(num_extensions);

    for (int32_t i = 0; i < num_extensions; i++) {
      extensions.extensions[i] = (const char *)fGetStringi(GL_EXTENSIONS, i);
    }
  } else if (fGetString != nullptr) {
    auto extensions_str = (const char *)fGetString(GL_EXTENSIONS);

    // split extensions_str
    std::string_view extensions_view{extensions_str};
    std::string_view delimiter{" "};

    size_t pos = 0;
    while ((pos = extensions_view.find(delimiter)) != std::string_view::npos) {
      std::string_view token = extensions_view.substr(0, pos);
      extensions.extensions.push_back(token);
      extensions_view.remove_prefix(pos + delimiter.length());
    }
  }

  if (extensions.extensions.empty()) {
    return false;
  }

  // GL_EXT_discard_framebuffer
  ext_discard_framebuffer = extensions.Contains("GL_EXT_discard_framebuffer");
  if (ext_discard_framebuffer) {
    fDiscardFramebufferEXT =
        (PFNGLDISCARDFRAMEBUFFEREXTPROC)loader("glDiscardFramebufferEXT");
  }

  // GL_EXT_multisampled_render_to_texture
  ext_multisampled_render_to_texture =
      extensions.Contains("GL_EXT_multisampled_render_to_texture");
  if (ext_multisampled_render_to_texture) {
    fFramebufferTexture2DMultisampleEXT =
        (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)loader(
            "glFramebufferTexture2DMultisampleEXT");
    fRenderbufferStorageMultisampleEXT =
        (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)loader(
            "glRenderbufferStorageMultisampleEXT");
  }

  // GL_OES_EGL_image_external
  oes_egl_image_external = extensions.Contains("GL_OES_EGL_image_external");
  if (oes_egl_image_external) {
    fEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)loader(
        "glEGLImageTargetTexture2DOES");
  }

  return true;
}

}  // namespace skity
