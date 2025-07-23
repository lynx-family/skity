// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "wgsl/ast/visitor.h"
#include "wgsl/function.h"

namespace wgx {
namespace msl {

class UniformCapture : public ast::AstVisitor {
 public:
  UniformCapture(Function* scope, ast::Function* func);

  ~UniformCapture() override = default;

  void Visit(ast::Attribute* attribute) override {}

  void Visit(ast::Expression* expression) override;

  void Visit(ast::Function* function) override;

  void Visit(ast::Identifier* identifier) override;

  void Visit(ast::Module* module) override {}

  void Visit(ast::Statement* statement) override;

  void Visit(ast::CaseSelector* case_selector) override;

  void Visit(ast::TypeDecl* type_decl) override {}

  void Visit(ast::StructMember* struct_member) override {}

  void Visit(ast::Variable* variable) override;

  void Capture();

  const std::vector<ast::Var*>& GetCapturedUniforms() { return uniforms_; }

 private:
  ast::Function* FindCalledFunction(const std::string_view& name);

 private:
  Function* scope_ = nullptr;
  ast::Function* func_ = nullptr;
  std::vector<ast::Var*> uniforms_;
};

}  // namespace msl
}  // namespace wgx
