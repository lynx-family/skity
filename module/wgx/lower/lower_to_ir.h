// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>
#include <unordered_map>

#include "ir/module.h"
#include "semantic/symbol.h"
#include "wgsl/ast/function.h"
#include "wgsl/ast/module.h"

namespace wgx {
namespace lower {

std::unique_ptr<ir::Module> LowerToIR(
    const ast::Module* module, const ast::Function* entry_point,
    const std::unordered_map<const ast::IdentifierExp*, semantic::Symbol*>&
        ident_symbols,
    const std::unordered_map<const ast::Identifier*, semantic::Symbol*>&
        decl_symbols);

}  // namespace lower
}  // namespace wgx
