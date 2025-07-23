// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_SHADER_GENERATOR_HPP
#define SRC_RENDER_HW_HW_SHADER_GENERATOR_HPP

#include <string>

namespace skity {

class HWShaderGenerator {
 public:
  virtual ~HWShaderGenerator() = default;

  virtual std::string GetVertexName() const = 0;
  virtual std::string GenVertexWGSL() const = 0;
  virtual const char* GetVertexEntryPoint() const = 0;

  virtual std::string GetFragmentName() const = 0;
  virtual std::string GenFragmentWGSL() const = 0;
  virtual const char* GetFragmentEntryPoint() const = 0;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_SHADER_GENERATOR_HPP
