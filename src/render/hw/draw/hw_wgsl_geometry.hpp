// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_WGSL_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_HW_WGSL_GEOMETRY_HPP

#include <skity/geometry/matrix.hpp>
#include <sstream>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/logging.hpp"

namespace skity {

struct HWDrawContext;
/**
 * This class represents a geometry. It is responsible for generating
 * the complete vertex shader or providing key code snippets for vertex shader
 * generation. It also manages the uploading and binding of vertex data and
 * uniform data.
 *
 * Due to some historical reasons, its behavior varies depending on the 'Flags'
 * set by its subclasses.
 *
 * If no flag is set, the default is 'Flags::kNone', meaning that the 'Geometry'
 * is responsible for generating the complete vertex shader and does not affect
 * the fragment shader.
 *
 * If 'Flags::kSnippet' is set, the 'Geometry' only provides some snippets used
 * to generate the vertex shader.
 *
 * If 'Flags::kAffectsFragment' is set, the 'Geometry' also affects the
 * generation of the fragment shader.
 */
class HWWGSLGeometry {
 public:
  struct Flags {
    enum Value : uint32_t {
      kNone = 0x0000,
      // Whether this provides a code snippet instead of a full shader.
      kSnippet = 0x0001,
      // Whether this affects the fragment shader generation;
      kAffectsFragment = 0x0002,
    };
  };

  HWWGSLGeometry(uint32_t flags = Flags::kNone) : flags_(flags) {}

  virtual ~HWWGSLGeometry() = default;

  virtual const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const = 0;

  /**
   * The vertex shader name.
   */
  virtual std::string GetShaderName() const = 0;

  /*
   * Generates the complete vertex shader. This method is called only when
   * 'Flags::kNone' is set. When 'Flags::kSnippet' is specified, vertex shader
   * generation is handled by 'HWWGSLShaderWriter', while 'Geometry' only
   * supplies the essential shader code snippets.
   */
  virtual std::string GenSourceWGSL() const { return ""; }

  /**
   * Supplies functions and data structs used by the vertex shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   */
  virtual void WriteVSFunctionsAndStructs(std::stringstream& ss) const {}

  /**
   * Supplies uniforms used by the vertex shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   */
  virtual void WriteVSUniforms(std::stringstream& ss) const {}

  /**
   * Supplies vertex input struct used by the vertex shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   */
  virtual void WriteVSInput(std::stringstream& ss) const {}

  /**
   * Supplies main logic of the vertex shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   */
  virtual void WriteVSMain(std::stringstream& ss) const {}

  /**
   * Supplies varings for vertex shader and fragment shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   *
   * According to the convention, all varying variables provided here must start
   * with the prefix 'v_'.
   */
  virtual std::optional<std::vector<std::string>> GetVarings() const {
    return std::nullopt;
  }

  /**
   * Supplies fragment shader name suffix. This method is called only when
   * 'Flags::kAffectsFragment' is specified.
   */
  virtual std::string GetFSNameSuffix() const { return GetShaderName(); }

  /**
   * Supplies functions and data structs used by the fragment shader. This
   * method is called only when 'Flags::kAffectsFragment' is specified.
   */
  virtual void WriteFSFunctionsAndStructs(std::stringstream& ss) const {}

  /**
   * Supplies mask alpha calculation used by the fragment shader. This
   * method is called only when 'Flags::kAffectsFragment' is specified.
   */
  virtual void WriteFSAlphaMask(std::stringstream& ss) const {}

  virtual const char* GetEntryPoint() const { return "vs_main"; }
  /**
   * Check if this geometry can be merged with other geometry.
   * If two geometries can be merged, they will share the same vertex buffer
   * and index buffer.
   *
   * @param other the other geometry to be checked.
   * @return true if can be merged, false otherwise.
   */
  virtual bool CanMerge(const HWWGSLGeometry* other) const { return false; }

  virtual void Merge(const HWWGSLGeometry* other) {}
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

  constexpr bool IsSnippet() const { return (flags_ & Flags::kSnippet) > 0; }

  constexpr bool AffectsFragment() const {
    return (flags_ & Flags::kAffectsFragment) > 0;
  }

 private:
  uint32_t flags_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_WGSL_GEOMETRY_HPP
