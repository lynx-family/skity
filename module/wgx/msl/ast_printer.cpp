// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "msl/ast_printer.h"

#include "msl/uniform_capture.h"

namespace wgx {
namespace msl {

AstPrinter::AstPrinter(const MslOptions& options, Function* func,
                       const std::optional<CompilerContext>& ctx)
    : options_(options),
      func_(func),
      ss_(),
      has_error_(false),
      buffer_index_(options.buffer_base_index),
      texture_index_(options.texture_base_index),
      sampler_index_(options.sampler_base_index),
      addtional_inputs_() {
  if (ctx) {
    if (buffer_index_ <= ctx->last_ubo_binding) {
      buffer_index_ = ctx->last_ubo_binding;
    }

    if (texture_index_ <= ctx->last_texture_binding) {
      texture_index_ = ctx->last_texture_binding;
    }

    if (sampler_index_ <= ctx->last_sampler_binding) {
      sampler_index_ = ctx->last_sampler_binding;
    }
  }
}

void AstPrinter::Visit(ast::Attribute* attribute) {}

void AstPrinter::Visit(ast::Expression* expression) {
  switch (expression->GetType()) {
    case ast::ExpressionType::kBoolLiteral: {
      auto literal = static_cast<ast::BoolLiteralExp*>(expression);

      ss_ << (literal->value ? "true" : "false");
      break;
    }

    case ast::ExpressionType::kIntLiteral: {
      auto literal = static_cast<ast::IntLiteralExp*>(expression);
      ss_ << std::to_string(literal->value);
    } break;

    case ast::ExpressionType::kFloatLiteral: {
      auto* literal = static_cast<ast::FloatLiteralExp*>(expression);
      ss_ << std::to_string(literal->value);
    } break;

    case ast::ExpressionType::kIdentifier: {
      auto* ident = static_cast<ast::IdentifierExp*>(expression);
      ident->ident->Accept(this);
    } break;

    case ast::ExpressionType::kFuncCall: {
      auto* call = static_cast<ast::FunctionCallExp*>(expression);

      if (call->ident->ident->name == "textureSample") {
        // textureSample(texture, sampler, uv) -> texture.sample(sampler, uv)
        call->args[0]->Accept(this);
        ss_ << ".sample(";
        call->args[1]->Accept(this);
        ss_ << ",";
        call->args[2]->Accept(this);
        ss_ << ")";
      } else if (call->ident->ident->name == "textureDimensions") {
        // textureDimensions(texture) -> uint2(texture.get_width(),
        // texture.get_height())
        ss_ << "uint2(";
        call->args[0]->Accept(this);
        ss_ << ".get_width()";
        ss_ << " , ";
        call->args[0]->Accept(this);
        ss_ << ".get_height()";
        ss_ << ")";
      } else {
        ast::Type type{call->ident};
        WriteType(type);

        if (type.IsArray()) {
          ss_ << "{";
        } else {
          ss_ << "(";
        }
        for (size_t i = 0; i < call->args.size(); ++i) {
          call->args[i]->Accept(this);

          if (i != call->args.size() - 1) {
            ss_ << ", ";
          }
        }
        if (function_inputs_.count(call->ident->ident->name)) {
          uint32_t param_index = call->args.size();
          const auto& uniforms = function_inputs_[call->ident->ident->name];

          for (auto* uniform : uniforms) {
            if (param_index != 0) {
              ss_ << ",";
            }

            uniform->name->Accept(this);

            param_index++;
          }
        }
        if (type.IsArray()) {
          ss_ << "}";
        } else {
          ss_ << ")";
        }
      }
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
  std::vector<Attribute> attributes = GetAttributes(
      function->attributes, ast::Type{}, AttrTarget::kFunction, false, false);

  if (function->IsEntryPoint()) {
    if (options_.msl_version_major >= 2 && options_.msl_version_minor >= 3) {
      ss_ << "[[";
      for (size_t i = 0; i < attributes.size(); ++i) {
        if (i != 0) {
          ss_ << ",";
        }

        ss_ << attributes[i];
      }

      ss_ << "]]";
    } else {
      if (function->GetPipelineStage() == ast::PipelineStage::kVertex) {
        ss_ << "vertex ";
      } else if (function->GetPipelineStage() ==
                 ast::PipelineStage::kFragment) {
        ss_ << "fragment ";
      }
    }
  } else {
    // gather all function used global uniform variable
    UniformCapture capture{func_, function};

    capture.Capture();

    auto captured_uniforms = capture.GetCapturedUniforms();

    if (!captured_uniforms.empty()) {
      function_inputs_[function->name->name] = captured_uniforms;
    }
  }

  ss_ << " ";
  WriteType(function->return_type);
  ss_ << " ";

  ss_ << function->name->name << "(";

  uint32_t param_index = 0;
  for (auto& param : function->params) {
    if (param_index++ != 0) {
      ss_ << ",";
    }

    WriteType(param->type);
    ss_ << " ";
    param->name->Accept(this);
    ss_ << " ";

    if (function->IsEntryPoint()) {
      auto p_attrs = GetAttributes(param->attributes, param->type,
                                   AttrTarget::kParameter, true, false);

      size_t attr_count = 0;
      ss_ << "[[";
      if (param->GetAttribute(ast::AttributeType::kBuiltin) == nullptr) {
        ss_ << "stage_in";
        attr_count++;
      }

      for (auto& attr : p_attrs) {
        if (attr.name == "flat") {
          if (param->type.IsBuiltin()) {
            // 'flat' attribute only applies to non-static data members
            continue;
          }
        }

        if (attr_count++ != 0) {
          ss_ << ",";
        }
        ss_ << attr;
      }

      ss_ << "]]";
    }
  }

  if (function->IsEntryPoint()) {
    for (auto& input : addtional_inputs_) {
      if (param_index++ != 0) {
        ss_ << ",";
      }

      bool is_const = true;

      if (input->type.expr) {
        const auto& type_name = input->type.expr->ident->name;
        if (type_name == "sampler" || type_name == "texture_1d" ||
            type_name == "texture_2d" || type_name == "texture_3d") {
          is_const = false;
        }
      }

      if (is_const) {
        ss_ << "constant ";
      }

      WriteType(input->type);
      if (is_const) {
        ss_ << "&";
      }
      ss_ << " ";

      input->name->Accept(this);

      auto attrs = GetAttributes(input->attributes, input->type,
                                 AttrTarget::kParameter, true, false);
      if (!attrs.empty()) {
        ss_ << " [[";
        for (size_t i = 0; i < attrs.size(); ++i) {
          if (i != 0) {
            ss_ << ",";
          }

          ss_ << attrs[i];
        }
        ss_ << "]]";
      }
    }
  } else if (function_inputs_.count(function->name->name)) {
    const auto& uniforms = function_inputs_[function->name->name];

    for (auto& uniform : uniforms) {
      ss_ << ",";

      bool is_const = true;

      if (uniform->type.expr) {
        const auto& type_name = uniform->type.expr->ident->name;
        if (type_name == "sampler" || type_name == "texture_1d" ||
            type_name == "texture_2d" || type_name == "texture_3d") {
          is_const = false;
        }
      }

      if (is_const) {
        ss_ << "constant ";
      }

      WriteType(uniform->type);
      if (is_const) {
        ss_ << "&";
      }
      ss_ << " ";

      uniform->name->Accept(this);
    }
  }

  ss_ << ")" << std::endl;

  function->body->Accept(this);
}

void AstPrinter::Visit(ast::Identifier* identifier) {
  ss_ << identifier->name;
  // prevent confict with the MSL keywords

  if (identifier->name == "vertex" || identifier->name == "fragment") {
    ss_ << "_1";
  }
}

void AstPrinter::Visit(ast::Statement* statement) {
  switch (statement->GetType()) {
    case ast::StatementType::kAssign: {
      auto assign = static_cast<ast::AssignStatement*>(statement);

      assign->lhs->Accept(this);
      ss_ << " ";
      if (assign->op) {
        ss_ << ast::op_to_string(*assign->op);
      }
      ss_ << "=";
      ss_ << " ";
      assign->rhs->Accept(this);
      ss_ << ";" << std::endl;
    } break;

    case ast::StatementType::kBlock: {
      auto block = static_cast<ast::BlockStatement*>(statement);
      ss_ << "{" << std::endl;

      for (auto& stmt : block->statements) {
        stmt->Accept(this);
      }

      ss_ << "}" << std::endl;
      ss_ << std::endl;
    } break;

    case ast::StatementType::kBreak:
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

      ss_ << "if ";
      ss_ << "(";
      if_stmt->condition->Accept(this);
      ss_ << ")";
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

      ss_ << ";" << std::endl;
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
    auto struct_decl = static_cast<ast::StructDecl*>(type_decl);

    // struct declaration
    ss_ << "struct ";
    struct_decl->name->Accept(this);
    ss_ << " {" << std::endl;

    for (auto& member : struct_decl->members) {
      member->Accept(this);
    }

    ss_ << "};" << std::endl;
    ss_ << std::endl;
  } else if (type_decl->GetType() == ast::TypeDeclType::kAlias) {
    auto alias_decl = static_cast<ast::Alias*>(type_decl);

    // alias declaration
    ss_ << "typedef ";
    alias_decl->name->Accept(this);
    ss_ << " ";
    WriteType(alias_decl->type);
    ss_ << ";" << std::endl;
  }
}

void AstPrinter::Visit(ast::StructMember* struct_member) {
  WriteType(struct_member->type);
  ss_ << " ";
  struct_member->name->Accept(this);
  ss_ << " ";

  WriteAttributes(struct_member->attributes, struct_member->type,
                  AttrTarget::kStructMember,
                  IsEntryPointInput(struct_member->parent->name->name),
                  IsEntryPointOutput(struct_member->parent->name->name));

  ss_ << ";" << std::endl;
}

void AstPrinter::Visit(ast::Variable* variable) {
  if (variable->GetType() == ast::VariableType::kVar) {
    auto* var = static_cast<ast::Var*>(variable);
    if (var->GetAttribute(ast::AttributeType::kBinding)) {
      // this is a uniform input
      addtional_inputs_.push_back(var);
      return;
    }
  }

  if (variable->GetType() == ast::VariableType::kConst) {
    ss_ << "constant ";
  }

  WriteType(variable->type);

  ss_ << " ";
  variable->name->Accept(this);

  if (variable->initializer) {
    ss_ << " = ";
    variable->initializer->Accept(this);
  }
}

bool AstPrinter::Write() {
  if (func_ == nullptr || !func_->GetFunction()->IsEntryPoint()) {
    return false;
  }

  if (func_->GetFunction()->GetPipelineStage() ==
      ast::PipelineStage::kCompute) {
    // not support compute shader for now
    return false;
  }

  // write header first
  ss_ << "#include <metal_stdlib>" << std::endl;
  ss_ << "#include <simd/simd.h>" << std::endl;
  ss_ << "using namespace metal;" << std::endl;
  ss_ << std::endl;

  // visit all declarations
  for (auto& decl : func_->GetTypeDecls()) {
    decl->Accept(this);
  }

  if (has_error_) {
    return false;
  }

  // visit all variables
  for (auto& decl : func_->GetGlobalDeclarations()) {
    decl->Accept(this);
    if (decl->GetType() == ast::VariableType::kConst) {
      ss_ << ";" << std::endl;
    }
  }

  if (has_error_) {
    return false;
  }

  // visit all functions
  for (auto& func : func_->GetFunctions()) {
    func->Accept(this);
  }

  if (has_error_) {
    return false;
  }

  // write main function
  func_->GetFunction()->Accept(this);

  return !has_error_;
}

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
    int dim = 0;

    if (name == "vec2") {
      dim = 2;
    } else if (name == "vec3") {
      dim = 3;
    } else if (name == "vec4") {
      dim = 4;
    }

    std::string_view prefix = "float";

    if (type.expr->ident->args.size() == 1 &&
        type.expr->ident->args[0]->GetType() ==
            ast::ExpressionType::kIdentifier) {
      const auto& t =
          static_cast<ast::IdentifierExp*>(type.expr->ident->args[0])
              ->ident->name;
      if (t == "bool") {
        prefix = "bool";
      } else if (t == "i32") {
        prefix = "int";
      } else if (t == "u32") {
        prefix = "uint";
      } else if (t == "f64") {
        prefix = "float";
      }
    }

    ss_ << prefix << dim;

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
      ss_ << "float4x4";
    } else if (name == "mat3x3") {
      ss_ << "float3x3";
    } else if (name == "mat2x2") {
      ss_ << "float2x2";
    } else {
      ss_ << name;
    }
  } else if (name == "texture_1d" || name == "texture_2d" ||
             name == "texture_3d") {
    ss_ << "texture";

    if (name == "texture_1d") {
      ss_ << "1d";
    } else if (name == "texture_2d") {
      ss_ << "2d";
    } else if (name == "texture_3d") {
      ss_ << "3d";
    }

    if (type.expr->ident->args.size() == 1 &&
        type.expr->ident->args[0]->GetType() ==
            ast::ExpressionType::kIdentifier) {
      const auto& type_name =
          static_cast<ast::IdentifierExp*>(type.expr->ident->args[0])
              ->ident->name;
      if (type_name == "f32") {
        ss_ << "<float>";
      } else if (type_name == "f64") {
        ss_ << "<float>";
      } else if (type_name == "i32") {
        ss_ << "<int>";
      } else if (type_name == "u32") {
        ss_ << "<uint>";
      }
    }
  } else if (name == "i32") {
    ss_ << "int";
  } else if (name == "u32") {
    ss_ << "uint";
  } else if (name == "f32") {
    ss_ << "float";
  } else if (type.IsArray()) {
    auto array = type.AsArray();

    auto inner_type = ast::Type{array.type};
    ss_ << "array<";
    WriteType(inner_type);
    ss_ << ", ";
    array.size->Accept(this);
    ss_ << ">";

  } else if (name == "atan") {
    ss_ << "precise::atan2";
  } else {
    ss_ << name;
  }
}

void AstPrinter::WriteAttributes(const std::vector<ast::Attribute*>& attributes,
                                 const ast::Type& type, AttrTarget target,
                                 bool entry_point_input,
                                 bool entry_point_output) {
  auto attrs = GetAttributes(attributes, type, target, entry_point_input,
                             entry_point_output);

  if (attrs.empty()) {
    return;
  }

  ss_ << "[[";

  for (size_t i = 0; i < attrs.size(); i++) {
    if (i > 0) {
      ss_ << ",";
    }

    ss_ << attrs[i];
  }

  ss_ << "]]";
}

std::vector<Attribute> AstPrinter::GetAttributes(
    const std::vector<ast::Attribute*>& attributes, const ast::Type& type,
    AttrTarget target, bool entry_point_input, bool entry_point_output) {
  std::vector<Attribute> attrs{};

  std::optional<uint32_t> group_index{};
  std::optional<uint32_t> binding_index{};
  std::optional<uint32_t> actual_index{};

  for (auto& attr : attributes) {
    if (attr->GetType() == ast::AttributeType::kBuiltin) {
      if (target == AttrTarget::kParameter ||
          target == AttrTarget::kStructMember) {
        auto builtin_attr = static_cast<ast::BuiltinAttribute*>(attr);

        if (builtin_attr->name == "position") {
          attrs.emplace_back(Attribute{"position"});
        } else if (builtin_attr->name == "vertex_index") {
          attrs.emplace_back(Attribute{"vertex_id"});
        } else if (builtin_attr->name == "instance_index") {
          attrs.emplace_back(Attribute{"instance_id"});
        }
      }
    } else if (attr->GetType() == ast::AttributeType::kGroup) {
      auto group_attr = static_cast<ast::GroupAttribute*>(attr);
      group_index = group_attr->index;
    } else if (attr->GetType() == ast::AttributeType::kBinding) {
      if (target == AttrTarget::kParameter ||
          target == AttrTarget::kStructMember) {
        auto binding_attr = static_cast<ast::BindingAttribute*>(attr);
        auto name = GetAttributeName(type);
        auto index = GetIndex(type, binding_attr->index);
        binding_index = binding_attr->index;
        actual_index = index;
        attrs.emplace_back(Attribute{name, index});
      }
    } else if (attr->GetType() == ast::AttributeType::kLocation) {
      if (target == AttrTarget::kParameter ||
          target == AttrTarget::kStructMember) {
        auto stage = func_->GetFunction()->GetPipelineStage();

        auto location_attr = static_cast<ast::LocationAttribute*>(attr);
        if (entry_point_input) {
          if (stage == ast::PipelineStage::kVertex) {
            attrs.emplace_back(Attribute{
                "attribute", static_cast<uint32_t>(location_attr->index)});
          } else {
            attrs.emplace_back(Attribute{
                "user", "locn", static_cast<uint32_t>(location_attr->index)});
          }
        } else if (entry_point_output) {
          if (stage == ast::PipelineStage::kVertex) {
            attrs.emplace_back(Attribute{
                "user", "locn", static_cast<uint32_t>(location_attr->index)});
          } else {
            attrs.emplace_back(Attribute{
                "color", static_cast<uint32_t>(location_attr->index)});
          }
        }
      }
    } else if (attr->GetType() == ast::AttributeType::kVertex ||
               attr->GetType() == ast::AttributeType::kFragment) {
      if (target == AttrTarget::kFunction) {
        attrs.emplace_back(Attribute{attr->GetName()});
      }
    } else if (attr->GetType() == ast::AttributeType::kInterpolate) {
      auto interpolate_attr = static_cast<ast::InterpolateAttribute*>(attr);

      // only support flat for now
      if (interpolate_attr->type == ast::InterpolateType::kFlat) {
        attrs.emplace_back(Attribute{"flat"});
      }
    }
  }

  if (group_index && binding_index && actual_index) {
    auto group = func_->GetBindGroup(*group_index);

    if (group == nullptr) {
      has_error_ = true;
      return {};
    }

    auto entry = group->GetEntry(*binding_index);

    if (entry == nullptr) {
      has_error_ = true;
      return {};
    }

    entry->index = *actual_index;
  }

  return attrs;
}

uint32_t AstPrinter::GetIndex(const ast::Type& type, uint32_t index) {
  if (type.expr == nullptr) {
    return 0;
  }

  auto type_name = type.expr->ident->name;

  if (type_name == "sampler") {
    return sampler_index_++;
  } else if (type_name == "texture_1d" || type_name == "texture_2d" ||
             type_name == "texture_3d") {
    return texture_index_++;
  } else {
    return buffer_index_++;
  }
}

std::string_view AstPrinter::GetAttributeName(const ast::Type& type) {
  if (type.expr == nullptr) {
    return "";
  }

  auto type_name = type.expr->ident->name;

  if (type_name == "sampler") {
    return "sampler";
  } else if (type_name == "texture_1d" || type_name == "texture_2d" ||
             type_name == "texture_3d") {
    return "texture";
  } else {
    return "buffer";
  }
}

bool AstPrinter::IsEntryPointInput(const std::string_view& type) {
  for (auto& params : func_->GetFunction()->params) {
    if (params->type.expr && params->type.expr->ident->name == type) {
      return true;
    }
  }

  return false;
}

bool AstPrinter::IsEntryPointOutput(const std::string_view& type) {
  if (func_->GetFunction()->return_type.expr == nullptr) {
    return false;
  }

  return func_->GetFunction()->return_type.expr->ident->name == type;
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

}  // namespace msl
}  // namespace wgx
