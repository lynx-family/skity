// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <sstream>

#include "wgsl/ast/visitor.h"
#include "wgsl/function.h"
#include "wgsl_cross.h"

namespace wgx {
namespace glsl {

class AstPrinter : public ast::AstVisitor {
 public:
  AstPrinter(const GlslOptions& options, Function* func,
             const std::optional<CompilerContext>& ctx)
      : options_(options), func_(func), ss_() {
    if (ctx) {
      ubo_index_ = ctx->last_ubo_binding;
      texture_index_ = ctx->last_texture_binding;
    }
  }

  ~AstPrinter() override = default;

  void Visit(ast::Attribute* attribute) override;

  void Visit(ast::Expression* expression) override;

  void Visit(ast::Function* function) override;

  void Visit(ast::Identifier* identifier) override;

  void Visit(ast::Module* module) override {}

  void Visit(ast::Statement* statement) override;

  void Visit(ast::CaseSelector* case_selector) override;

  void Visit(ast::TypeDecl* type_decl) override;

  void Visit(ast::StructMember* struct_member) override;

  void Visit(ast::Variable* variable) override;

  bool Write();

  std::string GetResult() const;

  uint32_t GetUboIndex() const { return ubo_index_; }

  uint32_t GetTextureIndex() const { return texture_index_; }

 private:
  void WriteType(const ast::Type& type);

  void WriteAttribute(ast::Variable* variable, bool input);

  void WriteAttribute(ast::StructMember* struct_member, bool input);

  void WriteLocation(ast::Attribute* location, ast::PipelineStage stage,
                     bool input);

  void WriteUniformVariable(ast::Var* var);

  void WriteInput();

  void WriteOutput();

  void WriteMainFunc();

  bool CanUseUboSlotBinding() const;

  ast::BuiltinAttribute* GetBuiltinAttribute(
      const std::vector<ast::Attribute*>& attrs, const std::string_view& name);

  void RegisterBindGroupEntry(ast::Var* var);

  void RegisterSampler(ast::Expression* texture, ast::Expression* sampler);

  ShaderStage GetShaderStage() const;

  void WriteBuiltinVariable(ast::Parameter*,
                            ast::BuiltinAttribute* builtin_attr);

 private:
  GlslOptions options_;
  Function* func_;
  std::stringstream ss_;
  bool has_error_ = false;
  uint32_t ubo_index_ = 0;
  uint32_t texture_index_ = 0;
};

}  // namespace glsl
}  // namespace wgx