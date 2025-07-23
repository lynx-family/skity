// Copyright 2020 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "wgsl/ast/attribute.h"
#include "wgsl/ast/expression.h"
#include "wgsl/ast/function.h"
#include "wgsl/ast/identifier.h"
#include "wgsl/ast/module.h"
#include "wgsl/ast/statement.h"
#include "wgsl/ast/type_decl.h"
#include "wgsl/ast/variable.h"

namespace wgx {
namespace ast {

class AstVisitor {
 public:
  virtual ~AstVisitor() = default;

  virtual void Visit(Attribute* attribute) = 0;

  virtual void Visit(Expression* expression) = 0;

  virtual void Visit(Function* function) = 0;

  virtual void Visit(Identifier* identifier) = 0;

  virtual void Visit(Module* module) = 0;

  virtual void Visit(Statement* statement) = 0;

  virtual void Visit(CaseSelector* case_selector) = 0;

  virtual void Visit(TypeDecl* type_decl) = 0;

  virtual void Visit(StructMember* struct_member) = 0;

  virtual void Visit(Variable* variable) = 0;
};

}  // namespace ast
}  // namespace wgx