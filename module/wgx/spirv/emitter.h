// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <string>

#include "ir/module.h"

namespace wgx {
namespace spirv {

class Emitter {
 public:
  Emitter() = default;
  ~Emitter() = default;

  bool Emit(const ir::Module& module);

  std::string GetResult() const { return result_; }

 private:
  std::string result_ = {};
};

}  // namespace spirv
}  // namespace wgx
