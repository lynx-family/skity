// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <string_view>

#include "wgsl/ast/node.h"

namespace wgx {
namespace semantic {

enum class SymbolKind {
  kVar,
  kLet,
  kConst,
  kParameter,
  kFunction,
  kType,
  kStructMember,
  kBuiltinType,
  kBuiltinFunction,
};

struct Symbol {
  SymbolKind kind = SymbolKind::kBuiltinType;
  uint32_t id = 0;
  std::string_view original_name = {};
  const ast::Node* declaration = nullptr;
};

}  // namespace semantic
}  // namespace wgx
