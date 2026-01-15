// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_WGX_FILTER_HPP
#define SRC_RENDER_HW_DRAW_WGX_FILTER_HPP

#include <memory>
#include <skity/effect/color_filter.hpp>
#include <string>
#include <vector>

#include "src/render/hw/hw_pipeline_key.hpp"

namespace skity {

struct Command;
struct HWDrawContext;

/**
 * Common code generator for all ColorFilter shader.
 *
 * The entry point name of all ColorFilter shader is
 *  fn filter_color(input_color: vec4<f32>) -> vec4<f32>
 *
 * The fragment may or may not contains Uniforms
 */
class WGXFilterFragment {
 public:
  explicit WGXFilterFragment(std::string suffix) : suffix_(std::move(suffix)) {}

  virtual ~WGXFilterFragment() = default;

  virtual uint32_t InitBinding(uint32_t binding) = 0;

  virtual std::string GenSourceWGSL() const = 0;

  virtual HWColorFilterKeyType::Value GetType() const = 0;

  virtual std::optional<std::vector<uint32_t>> GetComposeKeys() const {
    return std::nullopt;
  }

  virtual void SetupBindGroup(Command* cmd, HWDrawContext* context) = 0;

  static std::unique_ptr<WGXFilterFragment> Make(ColorFilter* filter,
                                                 std::string suffix = "");

 protected:
  std::string GenFunctionSignature() const;

 protected:
  std::string suffix_ = "";
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_WGX_FILTER_HPP
