// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <sstream>
#include <unordered_map>

#include "msl/attribute.h"
#include "wgsl/ast/visitor.h"
#include "wgsl/function.h"
#include "wgsl_cross.h"

namespace wgx {
namespace msl {

class AstPrinter : public ast::AstVisitor {
  enum class AttrTarget {
    kStructMember,
    kFunction,
    kParameter,
  };

 public:
  AstPrinter(const MslOptions& options, Function* func,
             const std::optional<CompilerContext>& ctx);

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

  std::string GetResult() const { return ss_.str(); }

  uint32_t GetBufferIndex() const { return buffer_index_; }

  uint32_t GetTextureIndex() const { return texture_index_; }

  uint32_t GetSamplerIndex() const { return sampler_index_; }

 private:
  void WriteType(const ast::Type& type);

  void WriteAttributes(const std::vector<ast::Attribute*>& attributes,
                       const ast::Type& type, AttrTarget target,
                       bool entry_point_input, bool entry_point_output);

  std::vector<Attribute> GetAttributes(
      const std::vector<ast::Attribute*>& attributes, const ast::Type& type,
      AttrTarget target, bool entry_point_input, bool entry_point_output);

  uint32_t GetIndex(const ast::Type& type, uint32_t index);

  std::string_view GetAttributeName(const ast::Type& type);

  bool IsEntryPointInput(const std::string_view& type);

  bool IsEntryPointOutput(const std::string_view& type);

  ShaderStage GetShaderStage() const;

 private:
  MslOptions options_;
  Function* func_;
  std::stringstream ss_;
  bool has_error_;
  uint32_t buffer_index_;
  uint32_t texture_index_;
  uint32_t sampler_index_;
  std::vector<ast::Var*> addtional_inputs_;

  std::unordered_map<std::string_view, std::vector<ast::Var*>>
      function_inputs_{};
};

}  // namespace msl
}  // namespace wgx
