// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "semantic/scope.h"

namespace wgx {
namespace semantic {

Scope::Scope(Scope* parent) : parent_(parent) {}

bool Scope::Declare(Symbol* symbol) {
  if (symbol == nullptr) {
    return false;
  }

  key_storage_.emplace_back(symbol->original_name.begin(),
                            symbol->original_name.end());
  std::string_view key = key_storage_.back();
  auto [_, inserted] = symbols_.emplace(key, symbol);
  if (!inserted) {
    key_storage_.pop_back();
  }
  return inserted;
}

Symbol* Scope::Lookup(const std::string_view& name) const {
  const Scope* scope = this;

  while (scope != nullptr) {
    auto it = scope->symbols_.find(name);
    if (it != scope->symbols_.end()) {
      return it->second;
    }
    scope = scope->parent_;
  }

  return nullptr;
}

Symbol* Scope::LookupCurrent(const std::string_view& name) const {
  auto it = symbols_.find(name);
  if (it == symbols_.end()) {
    return nullptr;
  }

  return it->second;
}

}  // namespace semantic
}  // namespace wgx
