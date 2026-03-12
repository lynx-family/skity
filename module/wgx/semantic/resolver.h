// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>
#include <vector>

#include "semantic/bind_result.h"
#include "semantic/scope.h"
#include "wgsl/ast/module.h"
#include "wgsl/ast/visitor.h"

namespace wgx {
namespace semantic {

class Resolver : public ast::AstVisitor {
 public:
  explicit Resolver(ast::Module* module);
  ~Resolver() override = default;

  BindResult Resolve();

  void Visit(ast::Attribute* attribute) override;
  void Visit(ast::Expression* expression) override;
  void Visit(ast::Function* function) override;
  void Visit(ast::Identifier* identifier) override;
  void Visit(ast::Module* module) override;
  void Visit(ast::Statement* statement) override;
  void Visit(ast::CaseSelector* case_selector) override;
  void Visit(ast::TypeDecl* type_decl) override;
  void Visit(ast::StructMember* struct_member) override;
  void Visit(ast::Variable* variable) override;

 private:
  void PushScope();
  void PopScope();

  Symbol* NewSymbol(SymbolKind kind, std::string_view name,
                    const ast::Node* declaration);

  SymbolKind GetVariableSymbolKind(ast::Variable* variable) const;

  bool DeclareOrReport(Symbol* symbol, const char* kind_name);

  void RecordIdentifierSymbol(ast::Identifier* identifier, Symbol* symbol);

  void Report(const std::string& message);

  void ResolveType(ast::Type type);
  bool ResolveTypeName(ast::IdentifierExp* ident);
  void ReportTypeError(std::string_view type_name, const std::string& message);
  ast::IdentifierExp* RequireTypeIdentifierArg(std::string_view type_name,
                                               ast::Expression* expr,
                                               const char* argument_name);
  ast::IdentifierExp* RequireIdentifierArg(std::string_view type_name,
                                           ast::Expression* expr,
                                           const char* argument_name);
  bool ValidateVectorType(std::string_view type_name,
                          const std::vector<ast::Expression*>& args);
  bool ValidateMatrixType(std::string_view type_name,
                          const std::vector<ast::Expression*>& args);
  bool ValidateArrayType(std::string_view type_name,
                         const std::vector<ast::Expression*>& args);
  bool ValidateAtomicType(std::string_view type_name,
                          const std::vector<ast::Expression*>& args);
  bool ValidatePtrType(std::string_view type_name,
                       const std::vector<ast::Expression*>& args);
  bool ValidateTextureSamplerType(std::string_view type_name,
                                  const std::vector<ast::Expression*>& args);

 private:
  ast::Module* module_ = nullptr;
  BindResult* result_ = nullptr;

  std::vector<std::unique_ptr<Scope>> scopes_ = {};
  Scope* current_scope_ = nullptr;
};

}  // namespace semantic
}  // namespace wgx
