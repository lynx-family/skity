/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GL_INTERFACE_HPP
#define SRC_GPU_GL_GL_INTERFACE_HPP

#include <skity/macros.hpp>

#ifdef __GNUC__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnewline-eof"
#endif

#include <glad/gl.h>

#ifdef __GNUC__
#pragma clang diagnostic pop
#endif

/// Open this macro to enable gl validation
/// but it will cost more time on some driver
// #ifndef GL_VALIDATION
// #define GL_VALIDATION 1
// #endif

#include "src/logging.hpp"

namespace skity {

struct GLInterface {
  typedef void (*GLFuncPtr)();
  typedef GLFuncPtr (*GLGetProc)(const char* name);

  static GLInterface* GlobalInterface();
  static void InitGlobalInterface(void* proc_loader);

  PFNGLACTIVETEXTUREPROC fActiveTexture = nullptr;
  PFNGLATTACHSHADERPROC fAttachShader = nullptr;
  PFNGLBINDATTRIBLOCATIONPROC fBindAttribLocation = nullptr;
  PFNGLBINDBUFFERPROC fBindBuffer = nullptr;
  PFNGLBINDBUFFERRANGEPROC fBindBufferRange = nullptr;
  PFNGLBINDFRAMEBUFFERPROC fBindFramebuffer = nullptr;
  PFNGLBINDRENDERBUFFERPROC fBindRenderbuffer = nullptr;
  PFNGLBINDSAMPLERPROC fBindSampler = nullptr;
  PFNGLBINDTEXTUREPROC fBindTexture = nullptr;
  PFNGLBINDVERTEXARRAYPROC fBindVertexArray = nullptr;
  PFNGLBLENDCOLORPROC fBlendColor = nullptr;
  PFNGLBLENDEQUATIONPROC fBlendEquation = nullptr;
  PFNGLBLENDFUNCPROC fBlendFunc = nullptr;
  PFNGLBLITFRAMEBUFFERPROC fBlitFramebuffer = nullptr;
  PFNGLBUFFERDATAPROC fBufferData = nullptr;
  PFNGLBUFFERSUBDATAPROC fBufferSubData = nullptr;
  PFNGLCHECKFRAMEBUFFERSTATUSPROC fCheckFramebufferStatus = nullptr;
  PFNGLCLEARBUFFERFIPROC fClearBufferfi = nullptr;
  PFNGLCLEARBUFFERFVPROC fClearBufferfv = nullptr;
  PFNGLCLEARCOLORPROC fClearColor = nullptr;
  PFNGLCLEARPROC fClear = nullptr;
  PFNGLCLEARDEPTHFPROC fClearDepthf = nullptr;
  PFNGLCLEARSTENCILPROC fClearStencil = nullptr;
  PFNGLCOLORMASKPROC fColorMask = nullptr;
  PFNGLCOMPILESHADERPROC fCompileShader = nullptr;
  PFNGLCOPYTEXSUBIMAGE2DPROC fCopyTexSubImage2D = nullptr;
  PFNGLREADBUFFERPROC fReadBuffer = nullptr;
  PFNGLCREATEPROGRAMPROC fCreateProgram = nullptr;
  PFNGLCREATESHADERPROC fCreateShader = nullptr;
  PFNGLCULLFACEPROC fCullFace = nullptr;
  PFNGLDELETEBUFFERSPROC fDeleteBuffers = nullptr;
  PFNGLDELETEFRAMEBUFFERSPROC fDeleteFramebuffers = nullptr;
  PFNGLDELETEPROGRAMPROC fDeleteProgram = nullptr;
  PFNGLDELETERENDERBUFFERSPROC fDeleteRenderbuffers = nullptr;
  PFNGLDELETESHADERPROC fDeleteShader = nullptr;
  PFNGLDELETETEXTURESPROC fDeleteTextures = nullptr;
  PFNGLDELETEVERTEXARRAYSPROC fDeleteVertexArrays = nullptr;
  PFNGLDEPTHMASKPROC fDepthMask = nullptr;
  PFNGLDEPTHFUNCPROC fDepthFunc = nullptr;
  PFNGLDISABLEPROC fDisable = nullptr;
  PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC fDisableVertexArrayAttrib = nullptr;
  PFNGLDISABLEVERTEXATTRIBARRAYPROC fDisableVertexAttribArray = nullptr;
  PFNGLDRAWARRAYSINDIRECTPROC fDrawArraysIndirect = nullptr;
  PFNGLDRAWARRAYSINSTANCEDPROC fDrawArraysInstanced = nullptr;
  PFNGLDRAWARRAYSPROC fDrawArrays = nullptr;
  PFNGLDRAWBUFFERPROC fDrawBuffer = nullptr;
  PFNGLDRAWBUFFERSPROC fDrawBuffers = nullptr;
  PFNGLDRAWELEMENTSPROC fDrawElements = nullptr;
  PFNGLENABLEPROC fEnable = nullptr;
  PFNGLENABLEVERTEXATTRIBARRAYPROC fEnableVertexAttribArray = nullptr;
  PFNGLFLUSHPROC fFlush = nullptr;
  PFNGLFRAMEBUFFERRENDERBUFFERPROC fFramebufferRenderbuffer = nullptr;
  PFNGLFRAMEBUFFERTEXTURE2DPROC fFramebufferTexture2D = nullptr;
  PFNGLGENBUFFERSPROC fGenBuffers = nullptr;
  PFNGLGENFRAMEBUFFERSPROC fGenFramebuffers = nullptr;
  PFNGLGENRENDERBUFFERSPROC fGenRenderbuffers = nullptr;
  PFNGLGENTEXTURESPROC fGenTextures = nullptr;
  PFNGLGENERATEMIPMAPPROC fGenerateMipmap = nullptr;
  PFNGLGENVERTEXARRAYSPROC fGenVertexArrays = nullptr;
  PFNGLGETATTRIBLOCATIONPROC fGetAttribLocation = nullptr;
  PFNGLGETERRORPROC fGetError = nullptr;
  PFNGLGETINTEGERVPROC fGetIntegerv = nullptr;
  PFNGLGETPROGRAMINFOLOGPROC fGetProgramInfoLog = nullptr;
  PFNGLGETPROGRAMIVPROC fGetProgramiv = nullptr;
  PFNGLGETSHADERINFOLOGPROC fGetShaderInfoLog = nullptr;
  PFNGLGETSHADERIVPROC fGetShaderiv = nullptr;
  PFNGLGETSTRINGPROC fGetString = nullptr;
  PFNGLGETSTRINGIPROC fGetStringi = nullptr;
  PFNGLGETUNIFORMLOCATIONPROC fGetUniformLocation = nullptr;
  PFNGLGETUNIFORMBLOCKINDEXPROC fGetUniformBlockIndex = nullptr;
  PFNGLINVALIDATEFRAMEBUFFERPROC fInvalidateFramebuffer = nullptr;
  PFNGLLINKPROGRAMPROC fLinkProgram = nullptr;
  PFNGLPIXELSTOREIPROC fPixelStorei = nullptr;
  PFNGLREADPIXELSPROC fReadPixels = nullptr;
  PFNGLRENDERBUFFERSTORAGEPROC fRenderbufferStorage = nullptr;
  PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC fRenderbufferStorageMultisample =
      nullptr;
  PFNGLSCISSORPROC fScissor = nullptr;
  PFNGLSHADERSOURCEPROC fShaderSource = nullptr;
  PFNGLSTENCILFUNCPROC fStencilFunc = nullptr;
  PFNGLSTENCILFUNCSEPARATEPROC fStencilFuncSeparate = nullptr;
  PFNGLSTENCILMASKPROC fStencilMask = nullptr;
  PFNGLSTENCILMASKSEPARATEPROC fStencilMaskSeparate = nullptr;
  PFNGLSTENCILOPPROC fStencilOp = nullptr;
  PFNGLSTENCILOPSEPARATEPROC fStencilOpSeparate = nullptr;
  PFNGLTEXIMAGE2DMULTISAMPLEPROC fTexImage2DMultisample = nullptr;
  PFNGLTEXIMAGE2DPROC fTexImage2D = nullptr;
  PFNGLTEXPARAMETERIPROC fTexParameteri = nullptr;
  PFNGLTEXSUBIMAGE2DPROC fTexSubImage2D = nullptr;
  PFNGLUNIFORMBLOCKBINDINGPROC fUniformBlockBinding = nullptr;
  PFNGLUNIFORM1FPROC fUniform1f = nullptr;
  PFNGLUNIFORM1FVPROC fUniform1fv = nullptr;
  PFNGLUNIFORM1IPROC fUniform1i = nullptr;
  PFNGLUNIFORM1IVPROC fUniform1iv = nullptr;
  PFNGLUNIFORM2FPROC fUniform2f = nullptr;
  PFNGLUNIFORM2FVPROC fUniform2fv = nullptr;
  PFNGLUNIFORM2IPROC fUniform2i = nullptr;
  PFNGLUNIFORM2IVPROC fUniform2iv = nullptr;
  PFNGLUNIFORM3FPROC fUniform3f = nullptr;
  PFNGLUNIFORM3FVPROC fUniform3fv = nullptr;
  PFNGLUNIFORM4FPROC fUniform4f = nullptr;
  PFNGLUNIFORM4FVPROC fUniform4fv = nullptr;
  PFNGLUNIFORM4IPROC fUniform4i = nullptr;
  PFNGLUNIFORM4IVPROC fUniform4iv = nullptr;
  PFNGLUNIFORMMATRIX4FVPROC fUniformMatrix4fv = nullptr;
  PFNGLUSEPROGRAMPROC fUseProgram = nullptr;
  PFNGLVERTEXATTRIBPOINTERPROC fVertexAttribPointer = nullptr;
  PFNGLVIEWPORTPROC fViewport = nullptr;
  PFNGLGENSAMPLERSPROC fGenSamplers = nullptr;
  PFNGLDELETESAMPLERSPROC fDeleteSamplers = nullptr;
  PFNGLSAMPLERPARAMETERIPROC fSamplerParameteri = nullptr;
  PFNGLVERTEXATTRIBDIVISORPROC fVertexAttribDivisor = nullptr;
  PFNGLDRAWELEMENTSINSTANCEDPROC fDrawElementsInstanced = nullptr;

  PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC
  fFramebufferTexture2DMultisampleEXT = nullptr;
  PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC
  fRenderbufferStorageMultisampleEXT = nullptr;
  PFNGLDISCARDFRAMEBUFFEREXTPROC fDiscardFramebufferEXT = nullptr;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC fEGLImageTargetTexture2DOES = nullptr;

  bool CanUseMSAA();

  bool ext_discard_framebuffer = false;
  bool ext_multisampled_render_to_texture = false;
  bool oes_egl_image_external = false;

 private:
  bool LoadExtensions(GLADloadfunc loader);
};

#if defined(SKITY_LOG) && defined(GL_VALIDATION)

template <class F, class Enable = void>
struct GLFunctionDelegate;

template <class F, class Enable = void>
struct GLFunctionDelegate;

template <class R, class... A>
struct GLFunctionDelegate<R (*)(A...),
                          typename std::enable_if<std::is_void_v<R>>::type> {
  using FuncType = R (*)(A...);

  void Dispatch(FuncType f, const char* fname, const char* loc, int line,
                A... args) {
    f(std::forward<A>(args)...);

    auto gl_call_err = GLInterface::GlobalInterface()->fGetError();
    if (gl_call_err != 0) {
      LOGE("glError = 0x{:x} at {} line: {} by function {}", gl_call_err,
           StripPath(loc), line, fname);
    }
  }
};

template <class R, class... A>
struct GLFunctionDelegate<R (*)(A...),
                          typename std::enable_if<!std::is_void_v<R>>::type> {
  using FuncType = R (*)(A...);

  R Dispatch(FuncType f, const char* fname, const char* loc, int line,
             A... args) {
    R r = f(std::forward<A>(args)...);

    auto gl_call_err = GLInterface::GlobalInterface()->fGetError();
    if (gl_call_err != 0) {
      LOGE("glError = 0x{:x} at {} line: {} by function {}", gl_call_err,
           StripPath(loc), line, fname);
    }

    return r;
  }
};

#define GL_CALL(name, ...)                                                \
  GLFunctionDelegate<decltype(GLInterface::GlobalInterface()->f##name)>{} \
      .Dispatch(GLInterface::GlobalInterface()->f##name, #name, __FILE__, \
                __LINE__, ##__VA_ARGS__)

#else
#define GL_CALL(name, ...) GLInterface::GlobalInterface()->f##name(__VA_ARGS__)
#endif

}  // namespace skity

#endif  // SRC_GPU_GL_GL_INTERFACE_HPP
