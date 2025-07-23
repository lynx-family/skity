// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_WASM_WASM_CANVAS_HPP
#define SRC_RENDER_WASM_WASM_CANVAS_HPP

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <string>

namespace skity {

class WasmCanvas {
 public:
  WasmCanvas(std::unique_ptr<GPUContext> ctx,
             std::unique_ptr<GPUSurface> surface);

  ~WasmCanvas() = default;

  static std::unique_ptr<WasmCanvas> Create(std::string const& name,
                                            uint32_t width, uint32_t height);

  void Save();

  void Translate(float dx, float dy);

  void Restore();

  void DrawRect(Rect const& rect, Paint const& paint);

  void DrawPath(Path const& path, Paint const& paint);

  void DrawRRect(RRect const& rrect, Paint const& paint);

  void DrawRoundRect(Rect const& rect, float rx, float ry, Paint const& paint);

  void DrawCircle(float cx, float cy, float radius, Paint const& paint);

  void DrawTextBlob(std::shared_ptr<TextBlob> const& blob, float x, float y,
                    Paint const& paint);

  void Flush();

 private:
  skity::Canvas* GetCanvas();

 private:
  std::unique_ptr<GPUContext> gpu_ctx_;
  std::unique_ptr<GPUSurface> gpu_surface_;

  skity::Canvas* frame_canvas_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_WASM_WASM_CANVAS_HPP
