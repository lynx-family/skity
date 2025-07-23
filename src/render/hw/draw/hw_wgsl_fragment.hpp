// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_WGSL_FRAGMENT_HPP
#define SRC_RENDER_HW_DRAW_HW_WGSL_FRAGMENT_HPP

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"

namespace skity {

class HWDrawContext;

/**
 * This class represents a fragment which can generate the WGSL source code for
 * fragment stage. And handle the uniform data upload and binding.
 */
class HWWGSLFragment {
 public:
  virtual ~HWWGSLFragment() = default;
  /**
   * The fragment shader name.
   * This is also the key of the fragment shader in HWPipelineLib
   */
  virtual std::string GetShaderName() const = 0;

  virtual std::string GenSourceWGSL() const = 0;

  virtual const char* GetEntryPoint() const = 0;

  virtual uint32_t NextBindingIndex() const = 0;

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

  /**
   * Mark this fragment needs to do anti-alias.
   * Subclass can ignore this flag. If the subclass does not have color output.
   */
  void SetAntiAlias(bool aa) { contour_aa_ = aa; }

 protected:
  std::unique_ptr<WGXFilterFragment> filter_ = {};
  bool contour_aa_ = false;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_WGSL_FRAGMENT_HPP
