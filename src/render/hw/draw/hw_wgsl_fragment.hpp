// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_WGSL_FRAGMENT_HPP
#define SRC_RENDER_HW_DRAW_HW_WGSL_FRAGMENT_HPP

#include <sstream>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"

namespace skity {

class HWDrawContext;

/**
 * This class represents a fragment. It is responsible for generating
 * the complete fragment shader or providing key code snippets for fragment
 * shader generation. It also manages the uploading and binding of uniform data
 * for fragment stage.
 *
 * Due to some historical reasons, its behavior varies depending on the 'Flags'
 * set by its subclasses.
 *
 * If no flag is set, the default is 'Flags::kNone', meaning that the 'Fragment'
 * is responsible for generating the complete fragment shader and does not
 * affect the vertex shader.
 *
 * If 'Flags::kSnippet' is set, the 'Fragment' only provides some snippets used
 * to generate the fragment shader.
 *
 * If 'Flags::kAffectsVertex' is set, the 'Geometry' also affects the
 * generation of the vertex shader.
 */
class HWWGSLFragment {
 public:
  struct Flags {
    enum Value : uint32_t {
      kNone = 0x0000,
      // Whether this provides a code snippet instead of a full shader.
      kSnippet = 0x0001,
      // Whether this affects the vertex shader generation or its uniform
      // binding.
      kAffectsVertex = 0x0002,
    };
  };

  HWWGSLFragment(uint32_t flags = Flags::kNone) : flags_(flags) {}

  virtual ~HWWGSLFragment() = default;
  /**
   * The fragment shader name.
   */
  virtual std::string GetShaderName() const = 0;

  virtual const char* GetEntryPoint() const { return "fs_main"; }

  virtual uint32_t NextBindingIndex() const = 0;

  /*
   * Generates the complete fragment shader. This method is called only when
   * 'Flags::kNone' is set. When 'Flags::kSnippet' is specified, fragment shader
   * generation is handled by 'HWWGSLShaderWriter', while 'Fragment' only
   * supplies the essential shader code snippets.
   */
  virtual std::string GenSourceWGSL() const { return ""; }

  /**
   * Supplies functions and data structs used by the fragment shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   */
  virtual void WriteFSFunctionsAndStructs(std::stringstream& ss) const {}

  /**
   * Supplies uniforms used by the fragment shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   */
  virtual void WriteFSUniforms(std::stringstream& ss) const {}

  /**
   * Supplies main logic of the fragment shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   */
  virtual void WriteFSMain(std::stringstream& ss) const {}

  /**
   * Supplies varings for vertex shader and fragment shader. This
   * method is called only when 'Flags::kSnippet' is specified.
   *
   * According to the convention, all varying variables provided here must start
   * with the prefix 'f_'.
   */
  virtual std::optional<std::vector<std::string>> GetVarings() const {
    return std::nullopt;
  }

  /**
   * Supplies vertex shader name suffix. This method is called only when
   * 'Flags::kAffectsVertex' is specified.
   */
  virtual std::string GetVSNameSuffix() const { return GetShaderName(); }

  /**
   * Supplies functions and data structs used by the vertex shader. This
   * method is called only when 'Flags::kAffectsVertex' is specified.
   */
  virtual void WriteVSFunctionsAndStructs(std::stringstream& ss) const {};

  /**
   * Supplies uniforms used by the vertex shader. This
   * method is called only when 'Flags::kAffectsVertex' is specified.
   */
  virtual void WriteVSUniforms(std::stringstream& ss) const {};

  /**
   * Supplies the assignment for shading varyings. This
   * method is called only when 'Flags::kAffectsVertex' is specified.
   */
  virtual void WriteVSAssgnShadingVarings(std::stringstream& ss) const {};

  /**
   * Bind shading uniform data for vertex stage.  This method is called only
   * when 'Flags::kAffectsVertex' is specified.
   *
   * @param cmd the command to be filled.
   * @param context the draw context used to pass GPUContext and other
   * information
   * @param transform the transform matrix of the geometry.
   * @param clip_depth the clip depth of the geometry.
   * @param stencil_cmd the stencil command before this command. Null if there
   * is no stencil step before this command
   */
  virtual void BindVSUniforms(Command* cmd, HWDrawContext* context,
                              const Matrix& transform, float clip_depth,
                              Command* stencil_cmd) {}

  virtual bool CanMerge(const HWWGSLFragment* other) const { return false; }

  virtual void Merge(const HWWGSLFragment* other) {}

  /**
   * Fill the command with the uniform data.
   *
   * @param cmd the command to be filled.
   * @param context the draw context used to pass GPUContext and other
   * information
   */
  virtual void PrepareCMD(Command* cmd, HWDrawContext* context) = 0;

  void SetFilter(std::unique_ptr<WGXFilterFragment> filter) {
    filter_ = std::move(filter);
    filter_->InitBinding(NextBindingIndex());
  }

  WGXFilterFragment* GetFilter() const { return filter_.get(); }

  constexpr bool IsSnippet() const { return (flags_ & Flags::kSnippet) > 0; }

  constexpr bool AffectsVertex() const {
    return (flags_ & Flags::kAffectsVertex) > 0;
  }

 protected:
  std::unique_ptr<WGXFilterFragment> filter_ = {};

 private:
  uint32_t flags_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_WGSL_FRAGMENT_HPP
