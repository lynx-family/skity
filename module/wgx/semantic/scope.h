// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

#include "semantic/symbol.h"

namespace wgx {
namespace semantic {

class Scope {
 public:
  explicit Scope(Scope* parent);

  Scope* Parent() const { return parent_; }

  bool Declare(Symbol* symbol);

  Symbol* Lookup(const std::string_view& name) const;

  Symbol* LookupCurrent(const std::string_view& name) const;

 private:
  Scope* parent_ = nullptr;
  std::deque<std::string> key_storage_;
  std::unordered_map<std::string_view, Symbol*> symbols_;
};

}  // namespace semantic
}  // namespace wgx
