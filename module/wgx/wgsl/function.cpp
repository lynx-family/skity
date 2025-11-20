// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl/function.h"

namespace wgx {

class FunctionCreator : public ast::AstVisitor {
 public:
  FunctionCreator(ast::Module *module, ast::Function *func)
      : module_(module), func_(func) {}

  ~FunctionCreator() override = default;

  void GatherAllTypes() { this->Visit(this->func_); }

  void Visit(ast::Attribute *attribute) override {}

  void Visit(ast::Expression *expression) override {
    switch (expression->GetType()) {
      // ignore int bool float literal
      case ast::ExpressionType::kBoolLiteral:
      case ast::ExpressionType::kIntLiteral:
      case ast::ExpressionType::kFloatLiteral:
        break;

      case ast::ExpressionType::kIdentifier:
        this->Visit(static_cast<ast::IdentifierExp *>(expression)->ident);
        break;

      case ast::ExpressionType::kFuncCall: {
        auto *func_call = static_cast<ast::FunctionCallExp *>(expression);

        this->Visit(func_call->ident);

        for (auto *arg : func_call->args) {
          this->Visit(arg);
        }
      } break;

      case ast::ExpressionType::kParenExp: {
        auto *paren_exp = static_cast<ast::ParenExp *>(expression);
        for (auto *exp : paren_exp->exps) {
          this->Visit(exp);
        }
      } break;

      case ast::ExpressionType::kUnaryExp: {
        auto *unary_exp = static_cast<ast::UnaryExp *>(expression);
        this->Visit(unary_exp->exp);
      } break;

      case ast::ExpressionType::kIndexAccessor: {
        auto *index_accessor = static_cast<ast::IndexAccessorExp *>(expression);
        this->Visit(index_accessor->obj);
        this->Visit(index_accessor->idx);
      } break;

      case ast::ExpressionType::kMemberAccessor: {
        auto *member_accessor = static_cast<ast::MemberAccessor *>(expression);
        this->Visit(member_accessor->obj);
        this->Visit(member_accessor->member);
      } break;

      case ast::ExpressionType::kBinaryExp: {
        auto *binary_exp = static_cast<ast::BinaryExp *>(expression);
        this->Visit(binary_exp->lhs);
        this->Visit(binary_exp->rhs);
      } break;

      default:
        break;
    }
  }

  void Visit(ast::Function *function) override {
    // Visit parameters and return type
    for (auto *param : function->params) {
      this->Visit(param);
    }

    if (!function->return_type.IsBuiltin()) {
      this->Visit(function->return_type.expr);
    }

    // Visit body
    this->Visit(function->body);
  }

  void Visit(ast::Identifier *identifier) override {
    // try find type declaration first
    auto type_decl = module_->GetGlobalTypeDecl(identifier->name);

    if (type_decl != nullptr) {
      this->AddTypeDecl(type_decl);
      return;
    }

    // try find global variable
    auto global_var = module_->GetGlobalVariable(identifier->name);

    if (global_var != nullptr) {
      this->AddGlobalVariable(global_var);
      return;
    }

    // try find function
    auto function = module_->GetFunction(identifier->name);

    if (function != nullptr) {
      this->AddFunction(function);
      return;
    }

    // TODO: handle error here
    // If reach here, it means the identifier is not defined in this module
    // maybe this is a builtin type or function all it maybe a mistake
  }

  void Visit(ast::Module *module) override {}

  void Visit(ast::Statement *statement) override {
    switch (statement->GetType()) {
      case ast::StatementType::kAssign: {
        auto *assign_stmt = static_cast<ast::AssignStatement *>(statement);
        this->Visit(assign_stmt->rhs);
        this->Visit(assign_stmt->lhs);
      } break;

      case ast::StatementType::kBlock: {
        auto *block_stmt = static_cast<ast::BlockStatement *>(statement);
        for (auto *stmt : block_stmt->statements) {
          this->Visit(stmt);
        }
      } break;

      case ast::StatementType::kBreak:
      case ast::StatementType::kContinue:
      case ast::StatementType::kDiscard:
        break;

      case ast::StatementType::kCase: {
        auto *case_stmt = static_cast<ast::CaseStatement *>(statement);
        for (auto *selector : case_stmt->selectors) {
          this->Visit(selector);
        }

        this->Visit(case_stmt->body);
      } break;

      case ast::StatementType::kCall: {
        auto *call_stmt = static_cast<ast::CallStatement *>(statement);
        this->Visit(call_stmt->expr);
      } break;

      case ast::StatementType::kIf: {
        auto *if_stmt = static_cast<ast::IfStatement *>(statement);
        this->Visit(if_stmt->condition);
        this->Visit(if_stmt->body);
        if (if_stmt->else_stmt) {
          this->Visit(if_stmt->else_stmt);
        }
      } break;

      case ast::StatementType::kLoop: {
        auto *loop_stmt = static_cast<ast::LoopStatement *>(statement);
        this->Visit(loop_stmt->body);
        this->Visit(loop_stmt->continuing);
      } break;

      case ast::StatementType::kReturn: {
        auto *return_stmt = static_cast<ast::ReturnStatement *>(statement);

        if (return_stmt->value != nullptr) {
          this->Visit(return_stmt->value);
        }
      } break;

      case ast::StatementType::kSwitch: {
        auto *switch_stmt = static_cast<ast::SwitchStatement *>(statement);
        this->Visit(switch_stmt->condition);

        for (auto *case_stmt : switch_stmt->body) {
          this->Visit(case_stmt);
        }
      } break;

      case ast::StatementType::kVarDecl: {
        auto *var_decl = static_cast<ast::VarDeclStatement *>(statement);

        this->Visit(var_decl->variable);
      } break;

      case ast::StatementType::kIncDecl: {
        auto *inc_decl = static_cast<ast::IncrementDeclStatement *>(statement);
        this->Visit(inc_decl->lhs);
      } break;

      case ast::StatementType::kForLoop: {
        auto *for_loop = static_cast<ast::ForLoopStatement *>(statement);
        if (for_loop->initializer != nullptr) {
          this->Visit(for_loop->initializer);
        }

        if (for_loop->condition != nullptr) {
          this->Visit(for_loop->condition);
        }

        if (for_loop->continuing != nullptr) {
          this->Visit(for_loop->continuing);
        }

        this->Visit(for_loop->body);
      } break;

      case ast::StatementType::kWhileLoop: {
        auto *while_loop = static_cast<ast::WhileLoopStatement *>(statement);

        if (while_loop->condition != nullptr) {
          this->Visit(while_loop->condition);
        }

        this->Visit(while_loop->body);
      } break;

      case ast::StatementType::kBreakIf: {
        auto *break_if = static_cast<ast::BreakIfStatement *>(statement);
        this->Visit(break_if->condition);
      } break;

      default:
        break;
    }
  }

  void Visit(ast::CaseSelector *case_selector) override {
    if (case_selector->expr != nullptr) {
      this->Visit(case_selector->expr);
    }
  }

  void Visit(ast::TypeDecl *type_decl) override {
    switch (type_decl->GetType()) {
      case ast::TypeDeclType::kAlias:
        this->Visit(static_cast<ast::Alias *>(type_decl)->type.expr);
        break;

      case ast::TypeDeclType::kStruct: {
        auto *struct_decl = static_cast<ast::StructDecl *>(type_decl);

        for (auto *member : struct_decl->members) {
          this->Visit(member);
        }
      } break;
    }
  }

  void Visit(ast::StructMember *struct_member) override {
    this->Visit(struct_member->type.expr);
  }

  void Visit(ast::Variable *variable) override {
    if (!variable->type.IsBuiltin()) {
      this->Visit(variable->type.expr);
    }

    if (variable->initializer != nullptr) {
      this->Visit(variable->initializer);
    }
  }

  std::unique_ptr<Function> CreateFunction(MemoryLayout layout) {
    return std::make_unique<Function>(func_, std::move(type_decls_),
                                      std::move(global_declarations_),
                                      std::move(functions_), layout);
  }

 private:
  void AddTypeDecl(ast::TypeDecl *type_decl) {
    for (auto *td : type_decls_) {
      if (td == type_decl) {
        return;
      }
    }

    // need to gather the type it used if this is a StructDecl
    this->Visit(type_decl);

    type_decls_.push_back(type_decl);
  }

  void AddGlobalVariable(ast::Variable *variable) {
    for (auto *v : global_declarations_) {
      if (v == variable) {
        return;
      }
    }

    // also need to gather the type of the variable
    this->Visit(variable->type.expr);
    global_declarations_.push_back(variable);
  }

  void AddFunction(ast::Function *function) {
    if (function == func_) {
      // incase entry point function call itself
      return;
    }

    for (auto *f : functions_) {
      if (f == function) {
        return;
      }
    }

    // also need to gather the type of the function
    this->Visit(function);

    functions_.push_back(function);
  }

 private:
  ast::Module *module_;
  ast::Function *func_;

  std::vector<ast::TypeDecl *> type_decls_ = {};
  std::vector<ast::Variable *> global_declarations_ = {};
  std::vector<ast::Function *> functions_ = {};
};

std::unique_ptr<Function> Function::Create(ast::Module *module,
                                           ast::Function *func,
                                           MemoryLayout layout) {
  FunctionCreator creator(module, func);
  creator.GatherAllTypes();

  return creator.CreateFunction(layout);
}

ast::TypeDecl *Function::GetTypeDecl(const std::string_view &name) const {
  for (auto *type_decl : type_decls_) {
    if (type_decl->name->name == name) {
      return type_decl;
    }
  }

  return nullptr;
}

ast::Variable *Function::GetGlobalVariable(const std::string_view &name) const {
  for (auto *global_var : global_declarations_) {
    if (global_var->name->name == name) {
      return global_var;
    }
  }

  return nullptr;
}

BindGroup *Function::GetBindGroup(uint32_t group) {
  for (auto &bind_group : bind_groups_) {
    if (bind_group.group == group) {
      return &bind_group;
    }
  }

  return nullptr;
}

void Function::InitBindGroups() {
  wgx::ShaderStage stage = wgx::ShaderStage::ShaderStage_kNone;

  auto pipeline_stage = func_->GetPipelineStage();
  if (pipeline_stage == ast::PipelineStage::kVertex) {
    stage = wgx::ShaderStage::ShaderStage_kVertex;
  } else if (pipeline_stage == ast::PipelineStage::kFragment) {
    stage = wgx::ShaderStage::ShaderStage_kFragment;
  }

  // visit all global variable
  for (auto *global_var : global_declarations_) {
    if (global_var->attributes.empty() ||
        global_var->GetType() != ast::VariableType::kVar) {
      continue;
    }

    auto *var = static_cast<ast::Var *>(global_var);

    auto group = static_cast<ast::GroupAttribute *>(
        var->GetAttribute(ast::AttributeType::kGroup));
    auto binding = static_cast<ast::BindingAttribute *>(
        var->GetAttribute(ast::AttributeType::kBinding));

    if (group == nullptr || binding == nullptr) {
      continue;
    }

    auto &bind_group = GetOrCreateBindGroup(group->index);

    if (var->address_space) {
      // this is a uniform variable

      BindGroupEntry entry{
          BindingType::kUniformBuffer,
          static_cast<uint32_t>(binding->index),
      };
      entry.stage = stage;

      // currently we only use uniform buffer, so it can switch to std140 if
      // layout is kWGSL

      auto layout = layout_;
      if (layout == MemoryLayout::kWGSL) {
        layout = MemoryLayout::kStd140;
      }

      entry.type_definition = CreateTypeDefinition(var->type, this, layout);
      entry.name = var->name->name;
      bind_group.entries.push_back(entry);
    } else if (var->type.expr->ident->name == "texture_2d") {
      BindGroupEntry entry{
          BindingType::kTexture,
          static_cast<uint32_t>(binding->index),
      };
      entry.stage = stage;
      entry.name = var->name->name;
      bind_group.entries.push_back(entry);
    } else if (var->type.expr->ident->name == "sampler") {
      BindGroupEntry entry{
          BindingType::kSampler,
          static_cast<uint32_t>(binding->index),
      };
      entry.stage = stage;
      entry.name = var->name->name;
      bind_group.entries.push_back(entry);
    }
  }
}

BindGroup &Function::GetOrCreateBindGroup(uint32_t group) {
  for (auto &bind_group : bind_groups_) {
    if (bind_group.group == group) {
      return bind_group;
    }
  }

  bind_groups_.emplace_back(BindGroup{group, {}});

  return bind_groups_.back();
}

}  // namespace wgx
