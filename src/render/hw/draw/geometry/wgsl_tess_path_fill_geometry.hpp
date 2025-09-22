// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TESS_PATH_FILL_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TESS_PATH_FILL_GEOMETRY_HPP

#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>

#include "src/gpu/gpu_buffer.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"

namespace skity {

class WGSLTessPathFillGeometry : public HWWGSLGeometry {
 public:
  WGSLTessPathFillGeometry(const Path& path, const Paint& paint);

  ~WGSLTessPathFillGeometry() override = default;

  const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const override;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override;

  const char* GetEntryPoint() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

  static GPUBufferView CreateVertexBufferView(HWStageBuffer* stage_bufer);

  static GPUBufferView CreateIndexBufferView(HWStageBuffer* stage_bufer);

 private:
  const Path& path_;
  const Paint& paint_;
  std::vector<GPUVertexBufferLayout> layout_;
};

class WGSLGradientTessPathFill : public WGSLTessPathFillGeometry {
 public:
  WGSLGradientTessPathFill(const Path& path, const Paint& paint,
                           const Matrix& local_matrix);

  ~WGSLGradientTessPathFill() override = default;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override;

  const char* GetEntryPoint() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  Matrix local_matrix_;
};

class WGSLTextureTessPathFill : public WGSLTessPathFillGeometry {
 public:
  WGSLTextureTessPathFill(const Path& path, const Paint& paint,
                          const Matrix& local_matrix, float width,
                          float height);

  ~WGSLTextureTessPathFill() override = default;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override;

  const char* GetEntryPoint() const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  Matrix local_matrix_;
  float width_;
  float height_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TESS_PATH_FILL_GEOMETRY_HPP
