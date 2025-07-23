// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/wasm/wasm_canvas.hpp"

#include <EGL/egl.h>
#include <emscripten/html5_webgl.h>

#include <iostream>
#include <skity/gpu/gpu_context_gl.hpp>
#include <string>

namespace skity {

WasmCanvas::WasmCanvas(std::unique_ptr<GPUContext> ctx,
                       std::unique_ptr<GPUSurface> surface)
    : gpu_ctx_(std::move(ctx)), gpu_surface_(std::move(surface)) {}

std::unique_ptr<WasmCanvas> WasmCanvas::Create(std::string const& name,
                                               uint32_t width,
                                               uint32_t height) {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;
  attrs.minorVersion = 0;
  attrs.antialias = false;
  attrs.enableExtensionsByDefault = 1;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context =
      emscripten_webgl_create_context(name.c_str(), &attrs);

  double pdr = emscripten_get_device_pixel_ratio();

  emscripten_set_element_css_size(name.c_str(), width, height);
  emscripten_set_canvas_element_size(name.c_str(), width * pdr, height * pdr);

  emscripten_webgl_make_context_current(context);

  auto ctx = skity::GLContextCreate(
      reinterpret_cast<void*>(emscripten_webgl_get_proc_address));

  skity::GPUSurfaceDescriptorGL desc{};
  desc.backend = skity::GPUBackendType::kOpenGL;
  desc.width = width * pdr;
  desc.height = height * pdr;
  desc.content_scale = 1.f;
  desc.sample_count = 4;

  desc.surface_type = skity::GLSurfaceType::kFramebuffer;
  desc.gl_id = 0;
  desc.has_stencil_attachment = false;

  auto surface = ctx->CreateSurface(&desc);

  return std::make_unique<WasmCanvas>(std::move(ctx), std::move(surface));
}

void WasmCanvas::Save() { GetCanvas()->Save(); }

void WasmCanvas::Translate(float dx, float dy) {
  GetCanvas()->Translate(dx, dy);
}

void WasmCanvas::Restore() { GetCanvas()->Restore(); }

void WasmCanvas::DrawRect(Rect const& rect, Paint const& paint) {
  GetCanvas()->DrawRect(rect, paint);
}

void WasmCanvas::DrawPath(Path const& path, Paint const& paint) {
  GetCanvas()->DrawPath(path, paint);
}

void WasmCanvas::DrawRRect(RRect const& rrect, Paint const& paint) {
  GetCanvas()->DrawRRect(rrect, paint);
}

void WasmCanvas::DrawRoundRect(Rect const& rect, float rx, float ry,
                               Paint const& paint) {
  GetCanvas()->DrawRoundRect(rect, rx, ry, paint);
}

void WasmCanvas::DrawCircle(float cx, float cy, float radius,
                            Paint const& paint) {
  GetCanvas()->DrawCircle(cx, cy, radius, paint);
}

void WasmCanvas::DrawTextBlob(std::shared_ptr<TextBlob> const& blob, float x,
                              float y, Paint const& paint) {
  GetCanvas()->DrawTextBlob(blob, x, y, paint);
}

skity::Canvas* WasmCanvas::GetCanvas() {
  if (frame_canvas_ == nullptr) {
    frame_canvas_ = gpu_surface_->LockCanvas();
  }

  return frame_canvas_;
}

void WasmCanvas::Flush() {
  frame_canvas_->Flush();
  frame_canvas_ = nullptr;
  gpu_surface_->Flush();
}

}  // namespace skity
