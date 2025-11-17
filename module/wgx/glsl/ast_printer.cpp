// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "glsl/ast_printer.h"

namespace wgx {
namespace glsl {

static std::string skip_glsl_keywords(const std::string_view& name) {
  if (name == "out" || name == "in" || name == "main" || name == "input" ||
      name == "output") {
    std::string ret{name.begin(), name.end()};
    ret += "_1";
    return ret;
  } else {
    return std::string{name.begin(), name.end()};
  }
}

void AstPrinter::Visit(ast::Attribute* attribute) {
  switch (attribute->GetType()) {
    case ast::AttributeType::kLocation: {
      auto* location = static_cast<ast::LocationAttribute*>(attribute);
      ss_ << "layout(location =" << location->index << ") ";
    } break;
    case ast::AttributeType::kInterpolate: {
      auto* interpolate = static_cast<ast::InterpolateAttribute*>(attribute);

      if (interpolate->type == ast::InterpolateType::kFlat) {
        ss_ << "flat ";
      }

      // not handle other type and sampling
    } break;

    default:
      break;
  }
}

void AstPrinter::Visit(ast::Expression* expression) {
  switch (expression->GetType()) {
    case ast::ExpressionType::kBoolLiteral: {
      auto* literal = static_cast<ast::BoolLiteralExp*>(expression);
      ss_ << (literal->value ? "true" : "false");
    } break;

    case ast::ExpressionType::kIntLiteral: {
      auto* literal = static_cast<ast::IntLiteralExp*>(expression);
      ss_ << std::to_string(literal->value);
    } break;

    case ast::ExpressionType::kFloatLiteral: {
      auto* literal = static_cast<ast::FloatLiteralExp*>(expression);
      ss_ << std::to_string(literal->value);
    } break;

    case ast::ExpressionType::kIdentifier: {
      auto* ident = static_cast<ast::IdentifierExp*>(expression);
      ident->ident->Accept(this);

      auto global_var = func_->GetGlobalVariable(ident->ident->name);

      if (global_var && global_var->GetType() == ast::VariableType::kVar) {
        auto var = static_cast<ast::Var*>(global_var);
        if (var->address_space) {
          // this is a uniform variable
          ss_ << ".inner";
        }
      }
    } break;

    case ast::ExpressionType::kFuncCall: {
      auto* call = static_cast<ast::FunctionCallExp*>(expression);

      if (call->ident->ident->name == "textureSample") {
        // textureSample(texture, sampler, uv) -> texture(texture, uv)
        RegisterSampler(call->args[0], call->args[1]);

        ss_ << "texture(";
        call->args[0]->Accept(this);
        ss_ << ", ";
        call->args[2]->Accept(this);
        ss_ << ")";
        return;
      } else if (call->ident->ident->name == "textureDimensions") {
        // textureDimensions(texture, x) -> textureSize(texture, x)
        ss_ << "textureSize(";
        call->args[0]->Accept(this);
        ss_ << ", ";
        if (call->args.size() == 2) {
          call->args[1]->Accept(this);
        } else {
          ss_ << "0";
        }
        ss_ << ")";
        return;
      } else if (call->ident->ident->name == "select") {
        ss_ << "(";
        call->args[2]->Accept(this);
        ss_ << " ? ";
        call->args[1]->Accept(this);
        ss_ << " : ";
        call->args[0]->Accept(this);
        ss_ << ")";
        return;
      }

      ast::Type type{call->ident};

      if (type.IsArray()) {
        auto array = type.AsArray();
        ast::Type inner_type{array.type};

        WriteType(inner_type);
        ss_ << "[" << static_cast<ast::IntLiteralExp*>(array.size)->value
            << "]";
      } else {
        WriteType(type);
      }
      ss_ << "(";
      for (size_t i = 0; i < call->args.size(); ++i) {
        call->args[i]->Accept(this);

        if (i != call->args.size() - 1) {
          ss_ << ", ";
        }
      }
      ss_ << ")";
    } break;

    case ast::ExpressionType::kParenExp: {
      auto* paren = static_cast<ast::ParenExp*>(expression);
      ss_ << "(";

      for (auto* exp : paren->exps) {
        exp->Accept(this);
      }

      ss_ << ")";
    } break;

    case ast::ExpressionType::kUnaryExp: {
      auto* unary = static_cast<ast::UnaryExp*>(expression);
      switch (unary->op) {
        case ast::UnaryOp::kAddressOf:
          ss_ << "&";
          break;
        case ast::UnaryOp::kComplement:
          ss_ << "~";
          break;
        case ast::UnaryOp::kIndirection:
          ss_ << "*";
          break;
        case ast::UnaryOp::kNegation:
          ss_ << "-";
          break;
        case ast::UnaryOp::kNot:
          ss_ << "!";
          break;

        default:
          break;
      }
      unary->exp->Accept(this);
    } break;

    case ast::ExpressionType::kBinaryExp: {
      auto* binary = static_cast<ast::BinaryExp*>(expression);
      binary->lhs->Accept(this);

      ss_ << " ";
      ss_ << ast::op_to_string(binary->op);
      ss_ << " ";

      binary->rhs->Accept(this);
    } break;

    case ast::ExpressionType::kIndexAccessor: {
      auto* index = static_cast<ast::IndexAccessorExp*>(expression);
      index->obj->Accept(this);
      ss_ << "[";
      index->idx->Accept(this);
      ss_ << "]";
    } break;

    case ast::ExpressionType::kMemberAccessor: {
      auto* member = static_cast<ast::MemberAccessor*>(expression);

      member->obj->Accept(this);

      ss_ << ".";
      member->member->Accept(this);
    } break;

    default:
      break;
  }
}

void AstPrinter::Visit(ast::Function* function) {
  WriteType(function->return_type);

  ss_ << " ";
  function->name->Accept(this);
  ss_ << "(";

  for (size_t i = 0; i < function->params.size(); ++i) {
    function->params[i]->Accept(this);

    if (i != function->params.size() - 1) {
      ss_ << ", ";
    }
  }

  ss_ << ")" << std::endl;

  function->body->Accept(this);

  ss_ << std::endl;
}

void AstPrinter::Visit(ast::Identifier* identifier) {
  ss_ << skip_glsl_keywords(identifier->name);
}

void AstPrinter::Visit(ast::Statement* statement) {
  switch (statement->GetType()) {
    case ast::StatementType::kAssign: {
      auto* assign = static_cast<ast::AssignStatement*>(statement);
      assign->lhs->Accept(this);

      ss_ << " ";
      if (assign->op.has_value()) {
        ss_ << ast::op_to_string(*assign->op);
      }
      ss_ << "=";
      ss_ << " ";

      assign->rhs->Accept(this);
      ss_ << ";" << std::endl;
    } break;
    case ast::StatementType::kBlock: {
      auto* block = static_cast<ast::BlockStatement*>(statement);
      ss_ << "{" << std::endl;

      for (auto* stmt : block->statements) {
        stmt->Accept(this);
      }

      ss_ << "}" << std::endl;
    } break;

    case wgx::ast::StatementType::kBreak:
      ss_ << "break;" << std::endl;
      break;

    case ast::StatementType::kCase: {
      auto* case_stmt = static_cast<ast::CaseStatement*>(statement);

      for (auto* selector : case_stmt->selectors) {
        selector->Accept(this);
        ss_ << ":" << std::endl;
      }

      if (case_stmt->body) {
        case_stmt->body->Accept(this);
        ss_ << std::endl;
      }
    } break;

    case ast::StatementType::kCall: {
      auto* call = static_cast<ast::CallStatement*>(statement);
      call->expr->Accept(this);
      ss_ << ";" << std::endl;
    } break;

    case ast::StatementType::kContinue:
      ss_ << "continue;" << std::endl;
      break;

    case ast::StatementType::kDiscard:
      ss_ << "discard;" << std::endl;
      break;

    case ast::StatementType::kIf: {
      auto* if_stmt = static_cast<ast::IfStatement*>(statement);

      ss_ << "if (";
      if_stmt->condition->Accept(this);
      ss_ << ") ";
      if (if_stmt->body) {
        if_stmt->body->Accept(this);
      } else {
        ss_ << "{ }" << std::endl;
      }

      if (if_stmt->else_stmt) {
        ss_ << "else ";
        if_stmt->else_stmt->Accept(this);
      }
    } break;

    case ast::StatementType::kLoop: {
      auto* loop = static_cast<ast::LoopStatement*>(statement);
      ss_ << "while (true) {" << std::endl;

      loop->body->Accept(this);

      loop->continuing->Accept(this);
      ss_ << "}" << std::endl;
    } break;

    case ast::StatementType::kReturn: {
      auto* ret = static_cast<ast::ReturnStatement*>(statement);
      ss_ << "return ";

      if (ret->value) {
        ret->value->Accept(this);
      }
      ss_ << ";" << std::endl;
    } break;

    case ast::StatementType::kSwitch: {
      auto* sw = static_cast<ast::SwitchStatement*>(statement);

      ss_ << "switch ";
      if (sw->condition->GetType() != ast::ExpressionType::kParenExp) {
        ss_ << "(";
      }
      sw->condition->Accept(this);
      if (sw->condition->GetType() != ast::ExpressionType::kParenExp) {
        ss_ << ")";
      }
      ss_ << " {" << std::endl;

      for (auto* case_stmt : sw->body) {
        case_stmt->Accept(this);
      }

      ss_ << "}" << std::endl;

    } break;

    case ast::StatementType::kVarDecl: {
      auto* var_decl = static_cast<ast::VarDeclStatement*>(statement);
      var_decl->variable->Accept(this);
    } break;

    case ast::StatementType::kIncDecl: {
      auto* inc = static_cast<ast::IncrementDeclStatement*>(statement);
      inc->lhs->Accept(this);

      if (inc->increment) {
        ss_ << "++";
      } else {
        ss_ << "--";
      }
      ss_ << ";" << std::endl;
    } break;

    case ast::StatementType::kForLoop: {
      auto* for_loop = static_cast<ast::ForLoopStatement*>(statement);

      ss_ << "for (";
      if (for_loop->initializer) {
        for_loop->initializer->Accept(this);
        ss_.seekp(-1, ss_.cur);  // remove the std::endl
      } else {
        ss_ << ";";
      }

      if (for_loop->condition) {
        for_loop->condition->Accept(this);
      }

      ss_ << ";";

      if (for_loop->continuing) {
        for_loop->continuing->Accept(this);
        ss_.seekp(-2, ss_.cur);  // remove the semicolon ()
      }

      ss_ << ")" << std::endl;
      for_loop->body->Accept(this);
      ss_ << std::endl;
    } break;

    case ast::StatementType::kWhileLoop: {
      auto* while_loop = static_cast<ast::WhileLoopStatement*>(statement);
      ss_ << "while (" << std::endl;
      while_loop->condition->Accept(this);
      ss_ << ")" << std::endl;
      while_loop->body->Accept(this);
      ss_ << std::endl;
    } break;

    case ast::StatementType::kBreakIf: {
      auto* break_if = static_cast<ast::BreakIfStatement*>(statement);

      ss_ << "if (";
      break_if->condition->Accept(this);
      ss_ << ") break;" << std::endl;
    } break;

    default:
      break;
  }
}

void AstPrinter::Visit(ast::CaseSelector* case_selector) {
  if (case_selector->IsDefault()) {
    ss_ << "default";
  } else {
    ss_ << "case ";
    case_selector->expr->Accept(this);
  }
}

void AstPrinter::Visit(ast::TypeDecl* type_decl) {
  if (type_decl->GetType() == ast::TypeDeclType::kStruct) {
    auto* struct_decl = static_cast<ast::StructDecl*>(type_decl);
    ss_ << "struct ";
    struct_decl->name->Accept(this);
    ss_ << " {" << std::endl;

    for (auto* member : struct_decl->members) {
      member->Accept(this);
    }

    ss_ << "};" << std::endl;
  } else if (type_decl->GetType() == ast::TypeDeclType::kAlias) {
    auto* alias = static_cast<ast::Alias*>(type_decl);
    ss_ << "typedef ";
    alias->name->Accept(this);
    ss_ << " (" << alias->type.expr->ident->name << ");" << std::endl;
  }
}

void AstPrinter::Visit(ast::StructMember* struct_member) {
  ss_ << "\t";
  WriteType(struct_member->type);
  ss_ << " ";
  struct_member->name->Accept(this);

  if (struct_member->type.IsArray()) {
    auto array = struct_member->type.AsArray();
    ss_ << "[" << static_cast<ast::IntLiteralExp*>(array.size)->value << "]";
  }

  ss_ << ";" << std::endl;
}

void AstPrinter::Visit(ast::Variable* variable) {
  if (variable->GetType() == ast::VariableType::kConst) {
    auto* const_var = static_cast<ast::ConstVar*>(variable);

    ss_ << "const ";
    WriteType(const_var->type);

    ss_ << " ";
    variable->name->Accept(this);
    ss_ << " = ";

    const_var->initializer->Accept(this);

    ss_ << ";" << std::endl;
  } else if (variable->GetType() == ast::VariableType::kVar) {
    auto* var = static_cast<ast::Var*>(variable);

    if (var->address_space) {
      RegisterBindGroupEntry(var);
      WriteUniformVariable(var);
    } else {
      if (var->type.expr->ident->name == "sampler") {
        // skip sampler uniform since glsl doesn't support it in shader
        // the binding group register will be handled during statement visit
        return;
      }

      if (var->type.expr->ident->name == "texture_2d") {
        RegisterBindGroupEntry(var);
        texture_index_++;
        ss_ << "uniform ";
      }

      WriteType(var->type);

      ss_ << " ";

      var->name->Accept(this);

      if (var->type.IsArray()) {
        auto array = var->type.AsArray();
        ss_ << "[" << static_cast<ast::IntLiteralExp*>(array.size)->value
            << "]";
      }

      if (var->initializer) {
        ss_ << " = ";
        var->initializer->Accept(this);
      }

      ss_ << ";" << std::endl;
    }
  } else if (variable->GetType() == ast::VariableType::kParameter) {
    WriteType(variable->type);

    ss_ << " ";
    variable->name->Accept(this);
  } else if (variable->GetType() == ast::VariableType::kLet) {
    WriteType(variable->type);

    ss_ << " ";
    variable->name->Accept(this);

    if (variable->initializer) {
      ss_ << " = ";
      variable->initializer->Accept(this);
    }

    ss_ << ";" << std::endl;
  }
}

bool AstPrinter::Write() {
  // write version header first
  ss_ << "#version " << options_.major_version << options_.minor_version
      << "0 ";

  if (options_.standard == GlslOptions::Standard::kDesktop) {
    ss_ << "core" << std::endl;
  } else {
    ss_ << "es" << std::endl;
  }

  ss_ << std::endl;

  if (options_.standard == GlslOptions::Standard::kES &&
      func_->GetFunction()->GetPipelineStage() ==
          ast::PipelineStage::kFragment) {
    // GLES needs define the default precision
    ss_ << "precision highp float;" << std::endl;
    ss_ << "precision highp int;" << std::endl;
    ss_ << std::endl;
  }

  // write input of this entry point function
  WriteInput();

  // write output of this entry point function
  WriteOutput();

  if (has_error_) {
    return false;
  }

  ss_ << std::endl;

  // visit all type decls
  for (auto* type_decl : func_->GetTypeDecls()) {
    type_decl->Accept(this);
  }

  if (has_error_) {
    return false;
  }

  // visit all global declarations
  for (auto* global_decl : func_->GetGlobalDeclarations()) {
    global_decl->Accept(this);
  }

  if (has_error_) {
    return false;
  }

  // visit all functions
  for (auto* func : func_->GetFunctions()) {
    func->Accept(this);
  }

  if (has_error_) {
    return false;
  }

  // visit entry point function
  func_->GetFunction()->Accept(this);

  if (has_error_) {
    return false;
  }

  WriteMainFunc();

  return true;
}

std::string AstPrinter::GetResult() const { return ss_.str(); }

void AstPrinter::WriteType(const ast::Type& type) {
  if (type.expr == nullptr) {
    ss_ << "void";
    return;
  }

  if (type.expr->GetType() == ast::ExpressionType::kIntLiteral) {
    ss_ << "int";
    return;
  } else if (type.expr->GetType() == ast::ExpressionType::kFloatLiteral) {
    ss_ << "float";
    return;
  } else if (type.expr->GetType() == ast::ExpressionType::kBoolLiteral) {
    ss_ << "bool";
    return;
  }

  const auto& name = type.expr->ident->name;

  if (name == "vec2" || name == "vec3" || name == "vec4") {
    if (type.expr->ident->args.size() == 1 &&
        type.expr->ident->args[0]->GetType() ==
            ast::ExpressionType::kIdentifier) {
      const auto& type_name =
          static_cast<ast::IdentifierExp*>(type.expr->ident->args[0])
              ->ident->name;

      if (type_name == "bool") {
        ss_ << "b";
      } else if (type_name == "i32") {
        ss_ << "i";
      } else if (type_name == "u32") {
        ss_ << "u";
      } else if (type_name == "f64") {
        ss_ << "d";
      }
    }
    ss_ << name;

  } else if (name == "mat4x4" || name == "mat3x3" || name == "mat2x2" ||
             name == "mat2x3" || name == "mat2x4" || name == "mat4x2" ||
             name == "mat4x3" || name == "mat3x4" || name == "mat3x2") {
    if (type.expr->ident->args.size() == 1 &&
        type.expr->ident->args[0]->GetType() ==
            ast::ExpressionType::kIdentifier) {
      const auto& type_name =
          static_cast<ast::IdentifierExp*>(type.expr->ident->args[0])
              ->ident->name;

      if (type_name == "bool") {
        ss_ << "b";
      } else if (type_name == "i32") {
        ss_ << "i";
      } else if (type_name == "u32") {
        ss_ << "u";
      } else if (type_name == "f64") {
        ss_ << "d";
      }
    }

    if (name == "mat4x4") {
      ss_ << "mat4";
    } else if (name == "mat3x3") {
      ss_ << "mat3";
    } else if (name == "mat2x2") {
      ss_ << "mat2";
    } else {
      ss_ << name;
    }
  } else if (name == "i32") {
    ss_ << "int";
  } else if (name == "u32") {
    ss_ << "uint";
  } else if (name == "f32") {
    ss_ << "float";
  } else if (name == "texture_3d") {
    ss_ << "sampler3D";
  } else if (name == "texture_2d") {
    ss_ << "sampler2D";
  } else if (name == "texture_1d") {
    ss_ << "sampler1D";
  } else if (type.IsArray()) {
    auto array = type.AsArray();

    ast::Type inner_type{array.type};

    WriteType(inner_type);
  } else {
    ss_ << name;
  }
}

void AstPrinter::WriteAttribute(ast::Variable* variable, bool input) {
  auto pipeline_stage = func_->GetFunction()->GetPipelineStage();

  auto attr = variable->GetAttribute(ast::AttributeType::kLocation);

  if (attr) {
    WriteLocation(static_cast<ast::LocationAttribute*>(attr), pipeline_stage,
                  input);
  }

  auto interpolate = variable->GetAttribute(ast::AttributeType::kInterpolate);
  if (interpolate) {
    interpolate->Accept(this);
  }

  if (input) {
    ss_ << "in ";
  } else {
    ss_ << "out ";
  }

  WriteType(variable->type);

  ss_ << " ";

  if (input && pipeline_stage == ast::PipelineStage::kVertex) {
    ss_ << "in_";
  } else {
    ss_ << "vs_out_";
  }

  variable->name->Accept(this);
  ss_ << ";" << std::endl;
}

void AstPrinter::WriteAttribute(ast::StructMember* member, bool input) {
  auto pipeline_stage = func_->GetFunction()->GetPipelineStage();

  auto attr = member->GetAttribute(ast::AttributeType::kLocation);

  if (attr) {
    WriteLocation(attr, pipeline_stage, input);
  }

  auto interpolate = member->GetAttribute(ast::AttributeType::kInterpolate);
  if (interpolate) {
    interpolate->Accept(this);
  }

  if (input) {
    ss_ << "in ";
  } else {
    ss_ << "out ";
  }

  WriteType(member->type);

  ss_ << " ";

  if (input && pipeline_stage == ast::PipelineStage::kVertex) {
    ss_ << "in_";
  } else {
    ss_ << "vs_out_";
  }

  member->name->Accept(this);
  ss_ << ";" << std::endl;
}

void AstPrinter::WriteLocation(ast::Attribute* location,
                               ast::PipelineStage stage, bool input) {
  if (options_.standard == GlslOptions::Standard::kDesktop) {
    // OpenGL 3.3 needs location for both program and shader stage inputs and
    // outputs
    location->Accept(this);
  } else {
    if (options_.major_version == 3 && options_.minor_version > 0) {
      // OpenGL ES 3.1 and 3.2 needs location for both program and stage inputs
      // and outputs
      location->Accept(this);
    } else {
      if (stage == ast::PipelineStage::kVertex && input) {
        // OpenGL ES 3.0 needs location for program inputs
        location->Accept(this);
      } else if (stage == ast::PipelineStage::kFragment && !input) {
        // OpenGL ES 3.0 needs location for program outputs
        location->Accept(this);
      }
    }
  }
}

void AstPrinter::WriteUniformVariable(ast::Var* var) {
  if (var->address_space->GetType() != ast::ExpressionType::kIdentifier) {
    has_error_ = true;
    return;
  }

  auto* address_space =
      static_cast<ast::IdentifierExp*>(var->address_space)->ident;
  // currently only support uniform block
  if (address_space->name != "uniform") {
    has_error_ = true;
    return;
  }

  ss_ << "layout ( ";
  if (CanUseUboSlotBinding()) {
    ss_ << "binding = " << ubo_index_++ << ", ";
  } else {
    ubo_index_++;  // for reflaction
  }

  ss_ << "std140 ) uniform ";
  var->name->Accept(this);
  ss_ << "block_ubo"
      << " {" << std::endl;

  WriteType(var->type);

  ss_ << " inner ;" << std::endl;

  ss_ << "} ";
  var->name->Accept(this);
  ss_ << ";" << std::endl;
}

void AstPrinter::WriteInput() {
  auto entry_point = func_->GetFunction();

  for (auto& param : entry_point->params) {
    if (param->type.IsBuiltin() &&
        param->GetAttribute(ast::AttributeType::kLocation)) {
      WriteAttribute(param, true);
    } else {
      auto type_decl = func_->GetTypeDecl(param->type.expr->ident->name);

      if (type_decl == nullptr) {
        continue;
      }

      if (type_decl->GetType() == ast::TypeDeclType::kAlias) {
        type_decl = func_->GetTypeDecl(
            static_cast<ast::Alias*>(type_decl)->type.expr->ident->name);
      }

      if (type_decl == nullptr) {
        continue;
      }

      if (type_decl->GetType() == ast::TypeDeclType::kStruct) {
        auto struct_decl = static_cast<ast::StructDecl*>(type_decl);

        for (auto& member : struct_decl->members) {
          if (member->GetAttribute(ast::AttributeType::kLocation)) {
            WriteAttribute(member, true);
          }
        }
      }
    }
  }
}

void AstPrinter::WriteOutput() {
  auto entry_point = func_->GetFunction();

  const auto& type = entry_point->return_type;
  const auto& attrs = entry_point->return_type_attrs;

  if (type.IsBuiltin() && attrs.empty()) {
    if (entry_point->GetPipelineStage() == ast::PipelineStage::kVertex) {
      // vertex function must have at least one output for gl_Position
      has_error_ = true;
      return;
    }
  }

  if (type.IsBuiltin()) {
    if (type.expr->ident->name == "void") {
      return;
    }

    ast::LocationAttribute* attr = nullptr;

    for (auto& attr_ : attrs) {
      if (attr_->GetType() == ast::AttributeType::kLocation) {
        attr = static_cast<ast::LocationAttribute*>(attr_);
        break;
      }
    }

    if (attr == nullptr) {
      return;
    }

    WriteLocation(attr, entry_point->GetPipelineStage(), false);

    ss_ << " out ";

    WriteType(type);

    if (entry_point->GetPipelineStage() == ast::PipelineStage::kFragment) {
      ss_ << " fragColor;" << std::endl;
    } else {
      ss_ << " vsOut;" << std::endl;
    }
  } else {
    auto type_decl = func_->GetTypeDecl(type.expr->ident->name);

    if (type_decl == nullptr) {
      has_error_ = true;
      return;
    }

    if (type_decl->GetType() == ast::TypeDeclType::kAlias) {
      type_decl = func_->GetTypeDecl(
          static_cast<ast::Alias*>(type_decl)->type.expr->ident->name);
    }

    if (type_decl == nullptr ||
        type_decl->GetType() != ast::TypeDeclType::kStruct) {
      has_error_ = true;
      return;
    }

    auto struct_decl = static_cast<ast::StructDecl*>(type_decl);

    for (auto& member : struct_decl->members) {
      if (!member->GetAttribute(ast::AttributeType::kLocation)) {
        continue;
      }
      WriteAttribute(member, false);
    }
  }
}

void AstPrinter::WriteMainFunc() {
  auto entry_point_func = func_->GetFunction();

  auto stage = entry_point_func->GetPipelineStage();

  ss_ << "void main() {" << std::endl;

  std::vector<ast::Identifier*> in_put_params{};

  // init input variable
  for (auto param : entry_point_func->params) {
    if (param->type.IsBuiltin()) {
      WriteType(param->type);
      ss_ << " ";
      param->name->Accept(this);
      ss_ << " = ";

      if (param->GetAttribute(ast::AttributeType::kBuiltin)) {
        auto builtin_attr = static_cast<ast::BuiltinAttribute*>(
            param->GetAttribute(ast::AttributeType::kBuiltin));

        WriteBuiltinVariable(param, builtin_attr);
      } else {
        if (stage == ast::PipelineStage::kVertex) {
          ss_ << "in_";
        } else {
          ss_ << "vs_out_";
        }

        param->name->Accept(this);
      }
      ss_ << ";" << std::endl;

      in_put_params.push_back(param->name);
    } else {
      auto type_decl = func_->GetTypeDecl(param->type.expr->ident->name);

      if (type_decl == nullptr) {
        continue;
      }

      if (type_decl->GetType() != ast::TypeDeclType::kStruct) {
        continue;
      }

      auto struct_decl = static_cast<ast::StructDecl*>(type_decl);

      WriteType(param->type);
      ss_ << " ";
      param->name->Accept(this);
      ss_ << ";" << std::endl;

      for (auto member : struct_decl->members) {
        auto attr = static_cast<ast::LocationAttribute*>(
            member->GetAttribute(ast::AttributeType::kLocation));
        auto builtin_attr = static_cast<ast::BuiltinAttribute*>(
            member->GetAttribute(ast::AttributeType::kBuiltin));

        if (attr == nullptr && builtin_attr == nullptr) {
          continue;
        }

        param->name->Accept(this);
        ss_ << ".";
        member->name->Accept(this);
        ss_ << " = ";

        if (builtin_attr) {
          WriteBuiltinVariable(param, builtin_attr);
        } else {
          if (stage == ast::PipelineStage::kVertex) {
            ss_ << "in_";
          } else {
            ss_ << "vs_out_";
          }
          member->name->Accept(this);
        }
        ss_ << ";" << std::endl;
      }

      in_put_params.emplace_back(param->name);
    }
  }

  ss_ << std::endl;

  WriteType(entry_point_func->return_type);

  ss_ << " entry_point_out = ";

  entry_point_func->name->Accept(this);

  ss_ << "(";

  for (size_t i = 0; i < in_put_params.size(); i++) {
    in_put_params[i]->Accept(this);

    if (i != in_put_params.size() - 1) {
      ss_ << ", ";
    }
  }
  ss_ << ");" << std::endl;

  if (entry_point_func->return_type.IsBuiltin()) {
    if (stage == ast::PipelineStage::kVertex) {
      auto pos_attr =
          GetBuiltinAttribute(entry_point_func->return_type_attrs, "position");

      if (pos_attr == nullptr) {
        has_error_ = true;
        return;
      }

      ss_ << "gl_Position = entry_point_out;" << std::endl;
    } else if (stage == ast::PipelineStage::kFragment) {
      ss_ << "fragColor = entry_point_out;" << std::endl;
    } else {
      has_error_ = true;
      return;
    }
  } else {
    auto type_decl =
        func_->GetTypeDecl(entry_point_func->return_type.expr->ident->name);

    if (type_decl->GetType() != ast::TypeDeclType::kStruct) {
      has_error_ = true;
      return;
    }

    auto struct_decl = static_cast<ast::StructDecl*>(type_decl);

    for (auto& member : struct_decl->members) {
      if (stage == ast::PipelineStage::kVertex &&
          GetBuiltinAttribute(member->attributes, "position")) {
        ss_ << "gl_Position = entry_point_out.";
        member->name->Accept(this);
        ss_ << ";" << std::endl;
        continue;
      }
      auto location = static_cast<ast::LocationAttribute*>(
          member->GetAttribute(ast::AttributeType::kLocation));

      if (location == nullptr) {
        continue;
      }

      if (stage == ast::PipelineStage::kVertex) {
        ss_ << "vs_";
      }

      ss_ << "out_";
      member->name->Accept(this);
      ss_ << " = entry_point_out.";
      member->name->Accept(this);
      ss_ << ";" << std::endl;
    }
  }

  ss_ << "}" << std::endl;
}

bool AstPrinter::CanUseUboSlotBinding() const {
  if (options_.standard == GlslOptions::Standard::kDesktop) {
    return options_.major_version >= 4 && options_.minor_version >= 2;
  } else {
    return options_.major_version >= 3 && options_.minor_version >= 1;
  }
}

ast::BuiltinAttribute* AstPrinter::GetBuiltinAttribute(
    const std::vector<ast::Attribute*>& attrs, const std::string_view& name) {
  for (auto& attr : attrs) {
    if (attr->GetType() == ast::AttributeType::kBuiltin) {
      auto builtin_attr = static_cast<ast::BuiltinAttribute*>(attr);

      if (builtin_attr->name == name) {
        return builtin_attr;
      }
    }
  }

  return nullptr;
}

void AstPrinter::RegisterBindGroupEntry(ast::Var* var) {
  auto group = static_cast<ast::GroupAttribute*>(
      var->GetAttribute(ast::AttributeType::kGroup));
  auto binding = static_cast<ast::BindingAttribute*>(
      var->GetAttribute(ast::AttributeType::kBinding));

  if (group == nullptr || binding == nullptr) {
    has_error_ = true;
    return;
  }

  auto bind_group = func_->GetBindGroup(group->index);

  if (bind_group == nullptr) {
    has_error_ = true;
    return;
  }

  auto bind_entry = bind_group->GetEntry(binding->index);

  if (bind_entry == nullptr) {
    has_error_ = true;
    return;
  }

  if (var->address_space) {
    // this is a uniform variable
    bind_entry->index = ubo_index_;
    // when convert to glsl, the compiler append a suffix to the name to prevent
    // name conflict
    bind_entry->name += "block_ubo";
  } else if (var->type.expr->ident->name == "texture_2d") {
    bind_entry->index = texture_index_;
  }
}

void AstPrinter::RegisterSampler(ast::Expression* texture,
                                 ast::Expression* sampler) {
  if (texture->GetType() != ast::ExpressionType::kIdentifier ||
      sampler->GetType() != ast::ExpressionType::kIdentifier) {
    has_error_ = true;
    return;
  }

  auto texture_name = static_cast<ast::IdentifierExp*>(texture)->ident->name;
  auto sampler_name = static_cast<ast::IdentifierExp*>(sampler)->ident->name;

  auto texture_var = func_->GetGlobalVariable(texture_name);
  auto sampler_var = func_->GetGlobalVariable(sampler_name);

  if (texture_var == nullptr || sampler_var == nullptr) {
    has_error_ = true;
    return;
  }

  auto group_index = static_cast<ast::GroupAttribute*>(
                         texture_var->GetAttribute(ast::AttributeType::kGroup))
                         ->index;
  auto binding_index =
      static_cast<ast::BindingAttribute*>(
          texture_var->GetAttribute(ast::AttributeType::kBinding))
          ->index;

  auto group = func_->GetBindGroup(group_index);

  if (group == nullptr) {
    has_error_ = true;
    return;
  }

  auto texture_entry = group->GetEntry(binding_index);

  if (texture_entry == nullptr) {
    has_error_ = true;
    return;
  }

  auto sampler_group_index =
      static_cast<ast::GroupAttribute*>(
          sampler_var->GetAttribute(ast::AttributeType::kGroup))
          ->index;

  auto sampler_binding_index =
      static_cast<ast::BindingAttribute*>(
          sampler_var->GetAttribute(ast::AttributeType::kBinding))
          ->index;

  if (sampler_group_index != group_index) {
    group = func_->GetBindGroup(sampler_group_index);

    if (group == nullptr) {
      has_error_ = true;
      return;
    }
  }

  auto sampler_entry = group->GetEntry(sampler_binding_index);

  if (sampler_entry == nullptr) {
    has_error_ = true;
    return;
  }

  if (sampler_entry->units.has_value()) {
    auto& units = *sampler_entry->units;
    bool found = false;

    for (auto i : units) {
      if (i == texture_entry->index) {
        found = true;
        break;
      }
    }

    if (!found) {
      units.push_back(texture_entry->index);
    }

  } else {
    sampler_entry->units = {texture_entry->index};
  }
}

ShaderStage AstPrinter::GetShaderStage() const {
  switch (func_->GetFunction()->GetPipelineStage()) {
    case ast::PipelineStage::kVertex:
      return ShaderStage_kVertex;
    case ast::PipelineStage::kFragment:
      return ShaderStage_kFragment;
    default:
      return ShaderStage_kNone;
  }
}

void AstPrinter::WriteBuiltinVariable(ast::Parameter*,
                                      ast::BuiltinAttribute* builtin_attr) {
  if (builtin_attr->name == "position") {
    auto stage = func_->GetFunction()->GetPipelineStage();
    if (stage == ast::PipelineStage::kVertex) {
      ss_ << "gl_Position";
    } else {
      ss_ << "gl_FragCoord";
    }
  } else if (builtin_attr->name == "vertex_index") {
    ss_ << "uint(gl_VertexID)";
  } else if (builtin_attr->name == "instance_index") {
    ss_ << "uint(gl_InstanceID)";
  }
}

}  // namespace glsl
}  // namespace wgx
