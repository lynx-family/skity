// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <wgsl_cross.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "semantic/symbol.h"
#include "wgsl/ast/expression.h"
#include "wgsl/ast/identifier.h"

namespace wgx {
namespace semantic {

struct BindResult {
  std::unordered_map<const ast::IdentifierExp*, Symbol*> ident_symbols = {};
  std::unordered_map<const ast::Identifier*, Symbol*> decl_symbols = {};
  std::vector<std::unique_ptr<Symbol>> symbols = {};
  std::vector<Diagnosis> diagnostics = {};

  bool HasError() const { return !diagnostics.empty(); }
};

}  // namespace semantic
}  // namespace wgx
