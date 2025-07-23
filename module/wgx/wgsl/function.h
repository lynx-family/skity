// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>
#include <vector>

#include "wgsl/ast/pipeline_stage.h"
#include "wgsl/ast/visitor.h"
#include "wgsl/type_definition.h"
#include "wgsl_cross.h"

namespace wgx {

/**
 * Represent a EntryPoint Function, which is a function with `@fragment`,
 * `@compute` or `@vertex` attribute in WGSL.
 *
 * Inaddition, it also holds all the global variables used Type and function
 * declarations. So that the backend can generate the corresponding code.
 */
class Function {
 public:
  Function(ast::Function *func, std::vector<ast::TypeDecl *> decls,
           std::vector<ast::Variable *> global_decls,
           std::vector<ast::Function *> functions, MemoryLayout layout)
      : func_(func),
        type_decls_(std::move(decls)),
        global_declarations_(std::move(global_decls)),
        functions_(std::move(functions)),
        layout_(layout) {
    InitBindGroups();
  }

  ~Function() = default;

  static std::unique_ptr<Function> Create(ast::Module *module,
                                          ast::Function *func,
                                          MemoryLayout layout);

  ast::Function *GetFunction() const { return func_; }

  const std::vector<ast::TypeDecl *> &GetTypeDecls() const {
    return type_decls_;
  }

  const std::vector<ast::Variable *> &GetGlobalDeclarations() const {
    return global_declarations_;
  }

  const std::vector<ast::Function *> &GetFunctions() const {
    return functions_;
  }

  ast::TypeDecl *GetTypeDecl(const std::string_view &name) const;

  ast::Variable *GetGlobalVariable(const std::string_view &name) const;

  const std::vector<BindGroup> &GetBindGroups() const { return bind_groups_; }

  BindGroup *GetBindGroup(uint32_t group);

 private:
  void InitBindGroups();

  BindGroup &GetOrCreateBindGroup(uint32_t group);

 private:
  ast::Function *func_;

  std::vector<ast::TypeDecl *> type_decls_ = {};
  std::vector<ast::Variable *> global_declarations_ = {};
  std::vector<ast::Function *> functions_ = {};

  MemoryLayout layout_ = MemoryLayout::kStd140;

  std::vector<BindGroup> bind_groups_;
};

}  // namespace wgx