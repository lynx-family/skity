// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_WGSL_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_HW_WGSL_GEOMETRY_HPP

#include <skity/geometry/matrix.hpp>

#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class HWDrawContext;

/**
 * This class represents a geometry which can generate the WGSL source code for
 * vertex stage. And handle the vertex and uniform data upload and binding.
 */
class HWWGSLGeometry {
 public:
  virtual ~HWWGSLGeometry() = default;

  virtual const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const = 0;

  /**
   * The vertex shader name.
   * This is also the key of the vertex shader in HWPipelineLib
   */
  virtual std::string GetShaderName() const = 0;

  virtual std::string GenSourceWGSL() const = 0;

  virtual const char* GetEntryPoint() const = 0;

  /**
   * Fill the command with the vertex data and uniform data.
   *
   * @param cmd the command to be filled.
   * @param context the draw context used to pass GPUContext and other
   * information
   * @param transform the transform matrix of the geometry.
   * @param clip_depth the clip depth of the geometry.
   * @param stencil_cmd the stencil command before this command. Null if there
   * is no stencil step before this command
   */
  virtual void PrepareCMD(Command* cmd, HWDrawContext* context,
                          const Matrix& transform, float clip_depth,
                          Command* stencil_cmd) = 0;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_WGSL_GEOMETRY_HPP
