// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "semantic/resolver.h"

#include <string>

#include "wgsl/ast/function.h"
#include "wgsl/ast/statement.h"
#include "wgsl/ast/type_decl.h"
#include "wgsl/ast/variable.h"

namespace wgx {
namespace semantic {

namespace {

static constexpr const char* kBuiltinTypeNames[] = {
    "bool",
    "i32",
    "u32",
    "f32",
    "f16",
    "vec2",
    "vec3",
    "vec4",
    "mat2x2",
    "mat2x3",
    "mat2x4",
    "mat3x2",
    "mat3x3",
    "mat3x4",
    "mat4x2",
    "mat4x3",
    "mat4x4",
    "texture_1d",
    "texture_2d",
    "texture_2d_array",
    "texture_3d",
    "texture_cube",
    "texture_cube_array",
    "sampler",
    "sampler_comparison",
    "array",
    "atomic",
    "ptr",
};

static constexpr const char* kBuiltinFunctionNames[] = {
    "abs",
    "acos",
    "acosh",
    "all",
    "any",
    "arrayLength",
    "asin",
    "asinh",
    "atan",
    "atan2",
    "atanh",
    "atomicAdd",
    "atomicAnd",
    "atomicCompareExchangeWeak",
    "atomicExchange",
    "atomicLoad",
    "atomicMax",
    "atomicMin",
    "atomicOr",
    "atomicStore",
    "atomicSub",
    "atomicXor",
    "bitcast",
    "ceil",
    "clamp",
    "cos",
    "cosh",
    "countLeadingZeros",
    "countOneBits",
    "countTrailingZeros",
    "cross",
    "degrees",
    "determinant",
    "distance",
    "dot",
    "dFdx",
    "dFdy",
    "dpdx",
    "dpdxCoarse",
    "dpdxFine",
    "dpdy",
    "dpdyCoarse",
    "dpdyFine",
    "exp",
    "exp2",
    "extractBits",
    "faceForward",
    "firstLeadingBit",
    "firstTrailingBit",
    "floor",
    "fma",
    "fract",
    "frexp",
    "fwidth",
    "fwidthCoarse",
    "fwidthFine",
    "insertBits",
    "inverseSqrt",
    "inversesqrt",
    "ldexp",
    "length",
    "log",
    "log2",
    "max",
    "min",
    "mix",
    "modf",
    "normalize",
    "pack2x16float",
    "pack2x16snorm",
    "pack2x16unorm",
    "pack4x8snorm",
    "pack4x8unorm",
    "pow",
    "quantizeToF16",
    "radians",
    "reflect",
    "refract",
    "reverseBits",
    "round",
    "saturate",
    "select",
    "sign",
    "sin",
    "sinh",
    "smoothstep",
    "sqrt",
    "step",
    "storageBarrier",
    "tan",
    "tanh",
    "textureBarrier",
    "textureDimensions",
    "textureGather",
    "textureGatherCompare",
    "textureLoad",
    "textureNumLayers",
    "textureNumLevels",
    "textureNumSamples",
    "textureSample",
    "textureSampleBias",
    "textureSampleCompare",
    "textureSampleCompareLevel",
    "textureSampleGrad",
    "textureSampleLevel",
    "textureStore",
    "transpose",
    "trunc",
    "unpack2x16float",
    "unpack2x16snorm",
    "unpack2x16unorm",
    "unpack4x8snorm",
    "unpack4x8unorm",
    "workgroupBarrier",
};

bool IsBuiltinTypeName(const std::string_view& name) {
  for (const char* builtin : kBuiltinTypeNames) {
    if (name == builtin) {
      return true;
    }
  }
  return false;
}

bool IsAddressSpaceName(const std::string_view& name) {
  return name == "function" || name == "private" || name == "workgroup" ||
         name == "uniform" || name == "storage" || name == "handle";
}

bool IsAccessModeName(const std::string_view& name) {
  return name == "read" || name == "write" || name == "read_write";
}

bool IsScalarTypeName(const std::string_view& name) {
  return name == "bool" || name == "i32" || name == "u32" || name == "f32" ||
         name == "f16";
}

bool IsFloatScalarTypeName(const std::string_view& name) {
  return name == "f32" || name == "f16";
}

bool IsVectorTypeName(const std::string_view& name) {
  return name == "vec2" || name == "vec3" || name == "vec4";
}

bool IsMatrixTypeName(const std::string_view& name) {
  return name == "mat2x2" || name == "mat2x3" || name == "mat2x4" ||
         name == "mat3x2" || name == "mat3x3" || name == "mat3x4" ||
         name == "mat4x2" || name == "mat4x3" || name == "mat4x4";
}

bool IsTextureOrSamplerTypeName(const std::string_view& name) {
  return name == "texture_1d" || name == "texture_2d" ||
         name == "texture_2d_array" || name == "texture_3d" ||
         name == "texture_cube" || name == "texture_cube_array" ||
         name == "sampler" || name == "sampler_comparison";
}

bool IsBuiltinFunctionName(const std::string_view& name) {
  for (const char* builtin : kBuiltinFunctionNames) {
    if (name == builtin) {
      return true;
    }
  }
  return false;
}

const char* SymbolKindName(SymbolKind kind) {
  switch (kind) {
    case SymbolKind::kVar:
      return "var";
    case SymbolKind::kLet:
      return "let";
    case SymbolKind::kConst:
      return "const";
    case SymbolKind::kParameter:
      return "parameter";
    case SymbolKind::kFunction:
      return "function";
    case SymbolKind::kType:
      return "type";
    case SymbolKind::kStructMember:
      return "struct member";
    case SymbolKind::kBuiltinType:
      return "builtin type";
    case SymbolKind::kBuiltinFunction:
      return "builtin function";
  }

  return "symbol";
}

}  // namespace

Resolver::Resolver(ast::Module* module) : module_(module) {}

BindResult Resolver::Resolve() {
  BindResult result;
  result_ = &result;
  scopes_.clear();
  current_scope_ = nullptr;

  if (module_ == nullptr) {
    result_ = nullptr;
    return result;
  }

  PushScope();
  for (const char* name : kBuiltinTypeNames) {
    Symbol* symbol = NewSymbol(SymbolKind::kBuiltinType, name, nullptr);
    DeclareOrReport(symbol, "builtin type");
  }
  for (const char* name : kBuiltinFunctionNames) {
    Symbol* symbol = NewSymbol(SymbolKind::kBuiltinFunction, name, nullptr);
    DeclareOrReport(symbol, "builtin function");
  }

  PushScope();

  for (auto* type_decl : module_->type_decls) {
    if (type_decl == nullptr || type_decl->name == nullptr) {
      continue;
    }

    Symbol* symbol =
        NewSymbol(SymbolKind::kType, type_decl->name->name, type_decl);
    DeclareOrReport(symbol, "type");
    RecordIdentifierSymbol(type_decl->name, symbol);
  }

  for (auto* variable : module_->global_declarations) {
    if (variable == nullptr || variable->name == nullptr) {
      continue;
    }

    Symbol* symbol = NewSymbol(GetVariableSymbolKind(variable),
                               variable->name->name, variable);
    DeclareOrReport(symbol, SymbolKindName(symbol->kind));
    RecordIdentifierSymbol(variable->name, symbol);
  }

  for (auto* function : module_->functions) {
    if (function == nullptr || function->name == nullptr) {
      continue;
    }

    Symbol* symbol =
        NewSymbol(SymbolKind::kFunction, function->name->name, function);
    DeclareOrReport(symbol, "function");
    RecordIdentifierSymbol(function->name, symbol);
  }

  Visit(module_);

  PopScope();
  PopScope();
  result_ = nullptr;
  return result;
}

void Resolver::Visit(ast::Attribute* attribute) { (void)attribute; }

void Resolver::Visit(ast::Expression* expression) {
  if (expression == nullptr) {
    return;
  }

  switch (expression->GetType()) {
    case ast::ExpressionType::kBoolLiteral:
    case ast::ExpressionType::kIntLiteral:
    case ast::ExpressionType::kFloatLiteral:
    case ast::ExpressionType::kPhonyExp:
      return;

    case ast::ExpressionType::kIdentifier: {
      auto* ident_exp = static_cast<ast::IdentifierExp*>(expression);
      if (ident_exp->ident == nullptr) {
        return;
      }

      const auto ident_name = ident_exp->ident->name;
      Symbol* symbol = current_scope_->Lookup(ident_name);
      if (symbol == nullptr) {
        Report("unresolved identifier '" +
               std::string{ident_name.begin(), ident_name.end()} + "'");
        return;
      }

      result_->ident_symbols[ident_exp] = symbol;
      return;
    }

    case ast::ExpressionType::kFuncCall: {
      auto* call = static_cast<ast::FunctionCallExp*>(expression);
      if (call->ident != nullptr && call->ident->ident != nullptr) {
        const auto callee_name = call->ident->ident->name;
        Symbol* callee_symbol = current_scope_->Lookup(callee_name);
        if (callee_symbol != nullptr) {
          result_->ident_symbols[call->ident] = callee_symbol;
        } else {
          Report("unresolved identifier '" +
                 std::string{callee_name.begin(), callee_name.end()} + "'");
        }
      }

      for (auto* arg : call->args) {
        Visit(arg);
      }
      return;
    }

    case ast::ExpressionType::kParenExp: {
      auto* paren = static_cast<ast::ParenExp*>(expression);
      for (auto* exp : paren->exps) {
        Visit(exp);
      }
      return;
    }

    case ast::ExpressionType::kUnaryExp: {
      auto* unary = static_cast<ast::UnaryExp*>(expression);
      Visit(unary->exp);
      return;
    }

    case ast::ExpressionType::kIndexAccessor: {
      auto* index = static_cast<ast::IndexAccessorExp*>(expression);
      Visit(index->obj);
      Visit(index->idx);
      return;
    }

    case ast::ExpressionType::kMemberAccessor: {
      auto* member = static_cast<ast::MemberAccessor*>(expression);
      Visit(member->obj);
      // Member names are type-dependent and should not be resolved as lexical
      // identifiers in this pass.
      return;
    }

    case ast::ExpressionType::kBinaryExp: {
      auto* binary = static_cast<ast::BinaryExp*>(expression);
      Visit(binary->lhs);
      Visit(binary->rhs);
      return;
    }
  }
}

void Resolver::Visit(ast::Function* function) {
  if (function == nullptr) {
    return;
  }

  PushScope();

  for (auto* param : function->params) {
    if (param == nullptr || param->name == nullptr) {
      continue;
    }

    Symbol* symbol =
        NewSymbol(SymbolKind::kParameter, param->name->name, param);
    DeclareOrReport(symbol, "parameter");
    RecordIdentifierSymbol(param->name, symbol);

    ResolveType(param->type);
  }

  ResolveType(function->return_type);
  if (function->body != nullptr) {
    Visit(static_cast<ast::Statement*>(function->body));
  }

  PopScope();
}

void Resolver::Visit(ast::Identifier* identifier) { (void)identifier; }

void Resolver::Visit(ast::Module* module) {
  if (module == nullptr) {
    return;
  }

  for (auto* type_decl : module->type_decls) {
    Visit(type_decl);
  }

  for (auto* global : module->global_declarations) {
    ResolveType(global->type);
    if (global->initializer != nullptr) {
      Visit(global->initializer);
    }
  }

  for (auto* function : module->functions) {
    Visit(function);
  }
}

void Resolver::Visit(ast::Statement* statement) {
  if (statement == nullptr) {
    return;
  }

  switch (statement->GetType()) {
    case ast::StatementType::kAssign: {
      auto* assign = static_cast<ast::AssignStatement*>(statement);
      Visit(assign->lhs);
      Visit(assign->rhs);
      return;
    }

    case ast::StatementType::kBlock: {
      auto* block = static_cast<ast::BlockStatement*>(statement);
      PushScope();
      for (auto* stmt : block->statements) {
        Visit(stmt);
      }
      PopScope();
      return;
    }

    case ast::StatementType::kBreak:
    case ast::StatementType::kContinue:
    case ast::StatementType::kDiscard:
      return;

    case ast::StatementType::kCase: {
      auto* case_stmt = static_cast<ast::CaseStatement*>(statement);
      for (auto* selector : case_stmt->selectors) {
        Visit(selector);
      }
      Visit(static_cast<ast::Statement*>(case_stmt->body));
      return;
    }

    case ast::StatementType::kCall: {
      auto* call = static_cast<ast::CallStatement*>(statement);
      Visit(static_cast<ast::Expression*>(call->expr));
      return;
    }

    case ast::StatementType::kIf: {
      auto* if_stmt = static_cast<ast::IfStatement*>(statement);
      Visit(if_stmt->condition);
      Visit(static_cast<ast::Statement*>(if_stmt->body));
      if (if_stmt->else_stmt != nullptr) {
        Visit(if_stmt->else_stmt);
      }
      return;
    }

    case ast::StatementType::kLoop: {
      auto* loop = static_cast<ast::LoopStatement*>(statement);
      Visit(static_cast<ast::Statement*>(loop->body));
      Visit(static_cast<ast::Statement*>(loop->continuing));
      return;
    }

    case ast::StatementType::kReturn: {
      auto* ret = static_cast<ast::ReturnStatement*>(statement);
      if (ret->value != nullptr) {
        Visit(ret->value);
      }
      return;
    }

    case ast::StatementType::kSwitch: {
      auto* sw = static_cast<ast::SwitchStatement*>(statement);
      Visit(sw->condition);
      for (auto* case_stmt : sw->body) {
        Visit(static_cast<ast::Statement*>(case_stmt));
      }
      return;
    }

    case ast::StatementType::kVarDecl: {
      auto* var_decl = static_cast<ast::VarDeclStatement*>(statement);
      Visit(var_decl->variable);
      return;
    }

    case ast::StatementType::kIncDecl: {
      auto* inc = static_cast<ast::IncrementDeclStatement*>(statement);
      Visit(inc->lhs);
      return;
    }

    case ast::StatementType::kForLoop: {
      auto* for_loop = static_cast<ast::ForLoopStatement*>(statement);
      PushScope();
      if (for_loop->initializer != nullptr) {
        Visit(for_loop->initializer);
      }
      if (for_loop->condition != nullptr) {
        Visit(for_loop->condition);
      }
      if (for_loop->continuing != nullptr) {
        Visit(for_loop->continuing);
      }
      Visit(static_cast<ast::Statement*>(for_loop->body));
      PopScope();
      return;
    }

    case ast::StatementType::kWhileLoop: {
      auto* while_loop = static_cast<ast::WhileLoopStatement*>(statement);
      Visit(while_loop->condition);
      Visit(static_cast<ast::Statement*>(while_loop->body));
      return;
    }

    case ast::StatementType::kBreakIf: {
      auto* break_if = static_cast<ast::BreakIfStatement*>(statement);
      Visit(break_if->condition);
      return;
    }
  }
}

void Resolver::Visit(ast::CaseSelector* case_selector) {
  if (case_selector != nullptr && case_selector->expr != nullptr) {
    Visit(case_selector->expr);
  }
}

void Resolver::Visit(ast::TypeDecl* type_decl) {
  if (type_decl == nullptr) {
    return;
  }

  switch (type_decl->GetType()) {
    case ast::TypeDeclType::kAlias: {
      auto* alias = static_cast<ast::Alias*>(type_decl);
      ResolveType(alias->type);
      return;
    }

    case ast::TypeDeclType::kStruct: {
      auto* struct_decl = static_cast<ast::StructDecl*>(type_decl);
      PushScope();

      // Phase 1: resolve all member types before adding member names into
      // scope, so member identifiers never participate in type lookup.
      for (auto* member : struct_decl->members) {
        if (member == nullptr || member->name == nullptr) {
          continue;
        }
        Visit(member);
      }

      // Phase 2: declare member names for duplicate checking.
      for (auto* member : struct_decl->members) {
        if (member == nullptr || member->name == nullptr) {
          continue;
        }

        Symbol* symbol =
            NewSymbol(SymbolKind::kStructMember, member->name->name, member);
        DeclareOrReport(symbol, "struct member");
        RecordIdentifierSymbol(member->name, symbol);
      }
      PopScope();
      return;
    }
  }
}

void Resolver::Visit(ast::StructMember* struct_member) {
  if (struct_member == nullptr) {
    return;
  }

  ResolveType(struct_member->type);
}

void Resolver::Visit(ast::Variable* variable) {
  if (variable == nullptr || variable->name == nullptr) {
    return;
  }

  SymbolKind kind = GetVariableSymbolKind(variable);

  ResolveType(variable->type);

  if (variable->initializer != nullptr) {
    Visit(variable->initializer);
  }

  Symbol* symbol = NewSymbol(kind, variable->name->name, variable);
  DeclareOrReport(symbol, SymbolKindName(kind));
  RecordIdentifierSymbol(variable->name, symbol);

  if (variable->GetType() == ast::VariableType::kVar) {
    auto* var = static_cast<ast::Var*>(variable);
    if (var->address_space != nullptr) {
      auto* address_space =
          RequireIdentifierArg("var", var->address_space, "var address space");
      if (address_space != nullptr &&
          !IsAddressSpaceName(address_space->ident->name)) {
        ReportTypeError("var",
                        "var address space must be "
                        "function/private/workgroup/uniform/storage/handle");
      }
    }
    if (var->access != nullptr) {
      auto* access =
          RequireIdentifierArg("var", var->access, "var access mode");
      if (access != nullptr && !IsAccessModeName(access->ident->name)) {
        ReportTypeError("var", "var access mode must be read/write/read_write");
      }
    }
  }
}

void Resolver::PushScope() {
  scopes_.push_back(std::make_unique<Scope>(current_scope_));
  current_scope_ = scopes_.back().get();
}

void Resolver::PopScope() {
  if (scopes_.empty()) {
    current_scope_ = nullptr;
    return;
  }

  Scope* parent =
      current_scope_ != nullptr ? current_scope_->Parent() : nullptr;
  scopes_.pop_back();
  current_scope_ = parent;
}

Symbol* Resolver::NewSymbol(SymbolKind kind, std::string_view name,
                            const ast::Node* declaration) {
  auto symbol = std::make_unique<Symbol>();
  symbol->kind = kind;
  symbol->id = static_cast<uint32_t>(result_->symbols.size());
  symbol->original_name = name;
  symbol->declaration = declaration;

  Symbol* raw_ptr = symbol.get();
  result_->symbols.push_back(std::move(symbol));
  return raw_ptr;
}

SymbolKind Resolver::GetVariableSymbolKind(ast::Variable* variable) const {
  switch (variable->GetType()) {
    case ast::VariableType::kVar:
      return SymbolKind::kVar;
    case ast::VariableType::kConst:
      return SymbolKind::kConst;
    case ast::VariableType::kLet:
      return SymbolKind::kLet;
    case ast::VariableType::kParameter:
      return SymbolKind::kParameter;
  }

  return SymbolKind::kVar;
}

bool Resolver::DeclareOrReport(Symbol* symbol, const char* kind_name) {
  if (symbol == nullptr) {
    return false;
  }

  if (current_scope_ == nullptr) {
    Report("internal resolver error: missing active scope");
    return false;
  }

  if (current_scope_->Declare(symbol)) {
    return true;
  }

  Symbol* previous = current_scope_->LookupCurrent(symbol->original_name);

  std::string message = "duplicate ";
  message += kind_name;
  message += " declaration '";
  message.append(symbol->original_name.begin(), symbol->original_name.end());
  message += "'";

  if (previous != nullptr) {
    message += " (already declared as ";
    message += SymbolKindName(previous->kind);
    message += ")";
  }

  Report(message);
  return false;
}

void Resolver::RecordIdentifierSymbol(ast::Identifier* identifier,
                                      Symbol* symbol) {
  if (result_ == nullptr || identifier == nullptr || symbol == nullptr) {
    return;
  }

  result_->decl_symbols[identifier] = symbol;
}

void Resolver::Report(const std::string& message) {
  result_->diagnostics.push_back(Diagnosis{message, 0, 0});
}

void Resolver::ResolveType(ast::Type type) {
  if (type.expr == nullptr || type.expr->ident == nullptr) {
    return;
  }

  auto* type_ident = type.expr->ident;
  const auto type_name = type_ident->name;
  const auto& args = type_ident->args;

  if (IsVectorTypeName(type_name)) {
    ValidateVectorType(type_name, args);
    return;
  }

  if (IsMatrixTypeName(type_name)) {
    ValidateMatrixType(type_name, args);
    return;
  }

  if (type_name == "array") {
    ValidateArrayType(type_name, args);
    return;
  }

  if (type_name == "atomic") {
    ValidateAtomicType(type_name, args);
    return;
  }

  if (type_name == "ptr") {
    ValidatePtrType(type_name, args);
    return;
  }

  if (IsTextureOrSamplerTypeName(type_name)) {
    ValidateTextureSamplerType(type_name, args);
    return;
  }

  if (!args.empty()) {
    ReportTypeError(type_name, "unexpected template arguments");
    return;
  }

  ResolveTypeName(type.expr);
}

bool Resolver::ResolveTypeName(ast::IdentifierExp* ident) {
  if (ident == nullptr || ident->ident == nullptr) {
    return false;
  }

  const auto name = ident->ident->name;
  Symbol* symbol = current_scope_->Lookup(name);
  if (symbol == nullptr) {
    Report("unresolved type '" + std::string{name.begin(), name.end()} + "'");
    return false;
  }
  if (symbol->kind != SymbolKind::kType &&
      symbol->kind != SymbolKind::kBuiltinType) {
    Report("identifier '" + std::string{name.begin(), name.end()} +
           "' is not a type");
    return false;
  }

  result_->ident_symbols[ident] = symbol;
  return true;
}

void Resolver::ReportTypeError(std::string_view type_name,
                               const std::string& message) {
  Report("invalid type declaration for '" +
         std::string{type_name.begin(), type_name.end()} + "': " + message);
}

ast::IdentifierExp* Resolver::RequireTypeIdentifierArg(
    std::string_view type_name, ast::Expression* expr,
    const char* argument_name) {
  if (expr == nullptr || expr->GetType() != ast::ExpressionType::kIdentifier) {
    ReportTypeError(type_name, std::string(argument_name) + " must be a type");
    return nullptr;
  }

  auto* ident = static_cast<ast::IdentifierExp*>(expr);
  if (ident->ident == nullptr) {
    ReportTypeError(type_name,
                    std::string(argument_name) + " must be a valid type");
    return nullptr;
  }
  return ident;
}

ast::IdentifierExp* Resolver::RequireIdentifierArg(std::string_view type_name,
                                                   ast::Expression* expr,
                                                   const char* argument_name) {
  if (expr == nullptr || expr->GetType() != ast::ExpressionType::kIdentifier) {
    ReportTypeError(type_name,
                    std::string(argument_name) + " must be an identifier");
    return nullptr;
  }

  auto* ident = static_cast<ast::IdentifierExp*>(expr);
  if (ident->ident == nullptr) {
    ReportTypeError(type_name,
                    std::string(argument_name) + " must be a valid identifier");
    return nullptr;
  }
  return ident;
}

bool Resolver::ValidateVectorType(std::string_view type_name,
                                  const std::vector<ast::Expression*>& args) {
  if (args.size() != 1) {
    ReportTypeError(type_name, "vector expects 1 template argument");
    return false;
  }

  auto* elem = RequireIdentifierArg(type_name, args[0], "vector element type");
  if (elem == nullptr) {
    return false;
  }

  if (!IsScalarTypeName(elem->ident->name)) {
    ReportTypeError(type_name,
                    "vector element type must be bool/i32/u32/f32/f16");
    return false;
  }
  return true;
}

bool Resolver::ValidateMatrixType(std::string_view type_name,
                                  const std::vector<ast::Expression*>& args) {
  if (args.size() != 1) {
    ReportTypeError(type_name, "matrix expects 1 template argument");
    return false;
  }

  auto* elem = RequireIdentifierArg(type_name, args[0], "matrix element type");
  if (elem == nullptr) {
    return false;
  }

  if (!IsFloatScalarTypeName(elem->ident->name)) {
    ReportTypeError(type_name, "matrix element type must be f32/f16");
    return false;
  }
  return true;
}

bool Resolver::ValidateArrayType(std::string_view type_name,
                                 const std::vector<ast::Expression*>& args) {
  if (args.size() != 2) {
    ReportTypeError(type_name, "array expects 2 template arguments");
    return false;
  }

  auto* elem =
      RequireTypeIdentifierArg(type_name, args[0], "array element type");
  if (elem == nullptr) {
    return false;
  }
  ast::Type elem_type;
  elem_type.expr = elem;
  ResolveType(elem_type);

  if (args[1] == nullptr ||
      args[1]->GetType() != ast::ExpressionType::kIntLiteral) {
    ReportTypeError(type_name, "array length must be an integer literal");
    return false;
  }

  auto* len = static_cast<ast::IntLiteralExp*>(args[1]);
  if (len->value <= 0) {
    ReportTypeError(type_name, "array length must be greater than 0");
    return false;
  }

  return true;
}

bool Resolver::ValidateAtomicType(std::string_view type_name,
                                  const std::vector<ast::Expression*>& args) {
  if (args.size() != 1) {
    ReportTypeError(type_name, "atomic expects 1 template argument");
    return false;
  }

  auto* elem = RequireIdentifierArg(type_name, args[0], "atomic element type");
  if (elem == nullptr) {
    return false;
  }

  if (elem->ident->name != "i32" && elem->ident->name != "u32") {
    ReportTypeError(type_name, "atomic element type must be i32/u32");
    return false;
  }
  return true;
}

bool Resolver::ValidatePtrType(std::string_view type_name,
                               const std::vector<ast::Expression*>& args) {
  if (args.size() != 2 && args.size() != 3) {
    ReportTypeError(type_name, "ptr expects 2 or 3 template arguments");
    return false;
  }

  auto* address_space =
      RequireIdentifierArg(type_name, args[0], "ptr address space");
  if (address_space == nullptr) {
    return false;
  }
  if (!IsAddressSpaceName(address_space->ident->name)) {
    ReportTypeError(type_name,
                    "ptr address space must be "
                    "function/private/workgroup/uniform/storage/handle");
    return false;
  }

  auto* pointee_type =
      RequireTypeIdentifierArg(type_name, args[1], "ptr pointee type");
  if (pointee_type == nullptr) {
    return false;
  }
  ast::Type pointee;
  pointee.expr = pointee_type;
  ResolveType(pointee);

  if (args.size() == 3) {
    auto* access = RequireIdentifierArg(type_name, args[2], "ptr access mode");
    if (access == nullptr) {
      return false;
    }
    if (!IsAccessModeName(access->ident->name)) {
      ReportTypeError(type_name,
                      "ptr access mode must be read/write/read_write");
      return false;
    }
  }

  return true;
}

bool Resolver::ValidateTextureSamplerType(
    std::string_view type_name, const std::vector<ast::Expression*>& args) {
  if (type_name == "sampler" || type_name == "sampler_comparison") {
    if (!args.empty()) {
      ReportTypeError(type_name,
                      "sampler type does not accept template arguments");
      return false;
    }
    return true;
  }

  if (args.size() != 1) {
    ReportTypeError(type_name, "texture type expects 1 template argument");
    return false;
  }

  auto* sampled_type =
      RequireIdentifierArg(type_name, args[0], "texture sampled type");
  if (sampled_type == nullptr) {
    return false;
  }
  if (!IsScalarTypeName(sampled_type->ident->name)) {
    ReportTypeError(type_name,
                    "texture sampled type must be bool/i32/u32/f32/f16");
    return false;
  }

  return true;
}

}  // namespace semantic
}  // namespace wgx
