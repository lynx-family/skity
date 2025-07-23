// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl/parser.h"

#include <optional>

#include "wgsl/ast/identifier.h"

namespace wgx {

ast::Module* Parser::BuildModule() {
  module_ = allocator_->Allocate<ast::Module>();

  TranslationUnit();

  if (has_error_) {
    return nullptr;
  } else {
    return module_;
  }
}

const Token& Parser::Peek(size_t offset) {
  auto index = token_index_ + offset;

  if (index >= tokens_.size()) {
    static Token eof(TokenType::kEOF, 0, 0);
    return eof;
  }

  return tokens_[index];
}

bool Parser::Consume(wgx::TokenType type) {
  if (Peek().Is(type)) {
    Advance();
    return true;
  } else {
    return false;
  }
}

bool Parser::Consume(wgx::TokenType type, std::string_view content) {
  const auto& token = Peek();

  if (token.Is(type) && token.content == content) {
    Advance();
    return true;
  }

  return false;
}

void Parser::Advance(size_t offset) { token_index_ += offset; }

void Parser::TranslationUnit() {
  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto state = GlobalDecl();

    if (state != State::kSuccess) {
      has_error_ = true;
    }
  }
}

Parser::State Parser::GlobalDecl() {
  if (Peek().Is(TokenType::kEOF) || Consume(TokenType::kSemicolon)) {
    return State::kSuccess;
  }

  auto attr_list = AttributeList();

  if (attr_list.state == State::kError) {
    has_error_ = true;
  }

  if (Peek().Is(TokenType::kEOF) || has_error_) {
    return State::kError;
  }

  std::vector<ast::Attribute*> attrs{};
  if (attr_list.state == State::kSuccess) {
    attrs = std::move(attr_list.GetValue());
  }

  {
    auto global_var = GlobalVariableDecl(attrs);

    if (global_var.state == State::kError) {
      return State::kError;
    }

    if (global_var.state == State::kSuccess) {
      if (!Consume(TokenType::kSemicolon)) {
        diagnosis_.message = "Expected ';' after global variable declaration";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return State::kError;
      }

      module_->AddGlobalDeclaration(global_var.GetValue());
      return State::kSuccess;
    }
  }

  {
    auto global_const = GlobalConstDecl(attrs);
    if (global_const.state == State::kError) {
      return State::kError;
    }

    if (global_const.state == State::kSuccess) {
      if (!Consume(TokenType::kSemicolon)) {
        diagnosis_.message = "Expected ';' after global const declaration";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return State::kError;
      }

      module_->AddGlobalDeclaration(global_const.GetValue());
    }
  }

  {
    auto ta = TypeAliasDecl();
    if (ta.state == State::kError) {
      return State::kError;
    }

    if (ta.state == State::kSuccess) {
      if (!attrs.empty()) {
        diagnosis_.message = "Type alias declaration must not have attribute";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return State::kError;
      }

      if (!Consume(TokenType::kSemicolon)) {
        diagnosis_.message = "Expected ';' after type alias declaration";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return State::kError;
      }

      module_->AddGlobalTypeDecl(ta.GetValue());

      return State::kSuccess;
    }
  }

  {
    auto struct_decl = StructDeclaration();
    if (struct_decl.state == State::kError) {
      return State::kError;
    }

    if (struct_decl.state == State::kSuccess) {
      if (!attrs.empty()) {
        diagnosis_.message = "Unexpected attribute in struct declaration";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return State::kError;
      }

      module_->AddGlobalTypeDecl(struct_decl.GetValue());

      return State::kSuccess;
    }
  }

  {
    auto func = FunctionDeclaration(attrs);

    if (func.state == State::kError) {
      return State::kError;
    }

    if (!attrs.empty()) {
      diagnosis_.message = "Unexpected attribute in function declaration";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return State::kError;
    }

    if (func.state == State::kSuccess) {
      module_->AddFunction(func.GetValue());
      return State::kSuccess;
    }
  }

  return State::kNotMatch;
}

Parser::Result<ast::Variable*> Parser::GlobalVariableDecl(
    std::vector<ast::Attribute*>& attrs) {
  using ReturnType = Result<ast::Variable*>;

  auto decl = VariableDeclaration();

  if (decl.state != State::kSuccess) {
    return ReturnType{decl.state};
  }

  auto& decl_info = decl.GetValue();

  ast::Expression* initializer = nullptr;

  if (Consume(TokenType::kEqual)) {
    auto expr = Expression();
    if (expr.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    initializer = expr.GetValue();
  }

  auto v = allocator_->Allocate<ast::Var>(
      decl_info.name, decl_info.type, decl_info.address_space, decl_info.access,
      initializer, std::move(attrs));

  return ReturnType{v};
}

Parser::Result<ast::Variable*> Parser::GlobalConstDecl(
    std::vector<ast::Attribute*>& attrs) {
  using ReturnType = Result<ast::Variable*>;

  if (Peek().Is(TokenType::kLet)) {
    // let not allowed in global scope
    diagnosis_.message = "Let not allowed in global scope";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  if (Peek().Is(TokenType::kOverride)) {
    // we not support override syntax for this version
    diagnosis_.message = "Override not support yet";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kConst)) {
    return ReturnType{State::kNotMatch};
  }

  auto decl = IdentWithOptionalTypeSpec(true);

  if (decl.state == State::kError) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kEqual)) {
    // const variable must have initializer
    diagnosis_.message = "Const variable must have initializer";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto initializer = Expression();

  if (initializer.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto& decl_info = decl.GetValue();

  auto const_var = allocator_->Allocate<ast::ConstVar>(
      decl_info.name, decl_info.type, initializer.GetValue(), std::move(attrs));

  return ReturnType{const_var};
}

Parser::Result<ast::Alias*> Parser::TypeAliasDecl() {
  using ReturnType = Result<ast::Alias*>;

  if (!Consume(TokenType::kAlias)) {
    return ReturnType{State::kNotMatch};
  }

  auto name = Identifier();

  if (name.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kEqual)) {
    diagnosis_.message = "Expected '=' after type alias name";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto type = TypeSpecifier();
  if (type.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto ta = allocator_->Allocate<ast::Alias>(name.GetValue(), type.GetValue());

  return ReturnType{ta};
}

Parser::Result<ast::StructMember*> Parser::StructMemberDecl() {
  using ReturnType = Result<ast::StructMember*>;

  auto attrs = AttributeList();

  AttrList attr_list{};
  if (attrs.state == State::kError) {
    return ReturnType{State::kError};
  }

  if (attrs.state == State::kSuccess) {
    attr_list = std::move(attrs.GetValue());
  }

  auto decl = IdentWithOptionalTypeSpec(false);

  if (decl.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto& decl_info = decl.GetValue();

  auto member = allocator_->Allocate<ast::StructMember>(
      decl_info.name, decl_info.type, std::move(attr_list));

  return ReturnType{member};
}

Parser::Result<StructMemberList> Parser::StructBodyDecl() {
  using ReturnType = Result<StructMemberList>;

  StructMemberList members{};

  if (!Consume(TokenType::kBraceLeft)) {
    return ReturnType{State::kNotMatch};
  }

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    if (Peek().Is(TokenType::kBraceRight)) {
      break;
    }

    auto member = StructMemberDecl();

    if (member.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (member.state == State::kNotMatch) {
      break;
    }

    members.emplace_back(member.GetValue());

    if (!Consume(TokenType::kComma)) {
      break;
    }
  }

  if (!Consume(TokenType::kBraceRight)) {
    diagnosis_.message = "Expected '}' after struct body";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  return ReturnType{members};
}

Parser::Result<ast::StructDecl*> Parser::StructDeclaration() {
  using ReturnType = Result<ast::StructDecl*>;

  if (!Consume(TokenType::kStruct)) {
    return ReturnType{State::kNotMatch};
  }

  auto name = Identifier();

  if (name.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto body = StructBodyDecl();

  if (body.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto sd = allocator_->Allocate<ast::StructDecl>(
      name.GetValue(), std::move(body.GetValue()), AttrList{});

  return ReturnType{sd};
}

Parser::Result<ast::Parameter*> Parser::Parameter() {
  using ReturnType = Result<ast::Parameter*>;

  auto attr_list = AttributeList();

  if (attr_list.state == State::kError) {
    return ReturnType{State::kError};
  }

  AttrList attrs{};
  if (attr_list.state == State::kSuccess) {
    attrs = std::move(attr_list.GetValue());
  }

  auto decl = IdentWithOptionalTypeSpec(false);

  if (decl.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto& decl_info = decl.GetValue();

  auto param = allocator_->Allocate<ast::Parameter>(
      decl_info.name, decl_info.type, std::move(attrs));

  return ReturnType{param};
}

Parser::Result<ParameterList> Parser::ParamList() {
  using ReturnType = Result<ParameterList>;

  ParameterList params{};

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    const auto& t = Peek();

    if (!t.Is(TokenType::kIdentifier) && !t.Is(TokenType::kAttr)) {
      break;
    }

    auto param = Parameter();
    if (param.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    params.emplace_back(param.GetValue());

    if (!Consume(TokenType::kComma)) {
      break;
    }
  }

  return ReturnType{params};
}

Parser::Result<Parser::FunctionHeader> Parser::FunctionHeaderDecl() {
  using ReturnType = Result<FunctionHeader>;

  if (!Consume(TokenType::kFn)) {
    return ReturnType{State::kNotMatch};
  }

  auto name = Identifier();

  if (name.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kParenLeft)) {
    diagnosis_.message = "Expected '(' after function name";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto params = ParamList();

  if (params.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kParenRight)) {
    diagnosis_.message = "Expected ')' after function parameter list";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  ast::Type return_type{};
  AttrList return_attrs{};

  if (Consume(TokenType::kArrow)) {
    auto attrs = AttributeList();
    if (attrs.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (attrs.state == State::kSuccess) {
      return_attrs = std::move(attrs.GetValue());
    }

    auto type = TypeSpecifier();
    if (type.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    return_type = type.GetValue();
  }

  return ReturnType{FunctionHeader{
      name.GetValue(),
      std::move(params.GetValue()),
      return_type,
      std::move(return_attrs),
  }};
}

Parser::Result<ast::BinaryOp> Parser::CompoundAssignmentOperator() {
  using ReturnType = Result<ast::BinaryOp>;

  if (Consume(TokenType::kPlusEqual)) {
    return ReturnType{ast::BinaryOp::kAdd};
  } else if (Consume(TokenType::kMinusEqual)) {
    return ReturnType{ast::BinaryOp::kSubtract};
  } else if (Consume(TokenType::kTimesEqual)) {
    return ReturnType{ast::BinaryOp::kMultiply};
  } else if (Consume(TokenType::kDivisionEqual)) {
    return ReturnType{ast::BinaryOp::kDivide};
  } else if (Consume(TokenType::kModuloEqual)) {
    return ReturnType{ast::BinaryOp::kModulo};
  } else if (Consume(TokenType::kAndEqual)) {
    return ReturnType{ast::BinaryOp::kAnd};
  } else if (Consume(TokenType::kOrEqual)) {
    return ReturnType{ast::BinaryOp::kOr};
  } else if (Consume(TokenType::kXorEqual)) {
    return ReturnType{ast::BinaryOp::kXor};
  } else if (Consume(TokenType::kShiftLeftEqual)) {
    return ReturnType{ast::BinaryOp::kShiftLeft};
  } else if (Consume(TokenType::kShiftRightEqual)) {
    return ReturnType{ast::BinaryOp::kShiftRight};
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<ast::Statement*> Parser::VariableUpdateStatement() {
  using ReturnType = Result<ast::Statement*>;

  if (Peek().Is(TokenType::kIdentifier) && Peek(1).Is(TokenType::kColon)) {
    diagnosis_.message = "UnExpected ':' after variable name";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  ast::Expression* lhs = nullptr;

  std::optional<ast::BinaryOp> op{};

  if (Consume(TokenType::kUnderscore)) {  // _ = expression
    if (!Consume(TokenType::kEqual)) {
      diagnosis_.message = "Expected '=' after '_'";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    lhs = allocator_->Allocate<ast::PhonyExpression>();
  } else {
    auto lhs_result = Expression();
    if (lhs_result.state != State::kSuccess) {
      return ReturnType{lhs_result.state};
    }

    lhs = lhs_result.GetValue();

    if (Consume(TokenType::kPlusPlus)) {
      return ReturnType{
          allocator_->Allocate<ast::IncrementDeclStatement>(lhs, true)};
    } else if (Consume(TokenType::kMinusMinus)) {
      return ReturnType{
          allocator_->Allocate<ast::IncrementDeclStatement>(lhs, false)};
    }

    auto compound_op_result = CompoundAssignmentOperator();

    if (compound_op_result.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (compound_op_result.state == State::kSuccess) {
      op = compound_op_result.GetValue();
    } else if (!Consume(TokenType::kEqual)) {
      diagnosis_.message = "Expected '=' after variable name";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }
  }

  auto rhs = Expression();

  if (rhs.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto stmt =
      allocator_->Allocate<ast::AssignStatement>(lhs, rhs.GetValue(), op);

  return ReturnType{stmt};
}

Parser::Result<ast::VarDeclStatement*> Parser::VariableStatement() {
  using ReturnType = Result<ast::VarDeclStatement*>;

  if (Consume(TokenType::kConst)) {
    auto typed_ident = IdentWithOptionalTypeSpec(true);

    if (!Consume(TokenType::kEqual)) {
      diagnosis_.message = "Expected '=' after const variable name";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto init = Expression();

    if (init.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    auto& type_info = typed_ident.GetValue();

    auto const_variable = allocator_->Allocate<ast::ConstVar>(
        type_info.name, type_info.type, init.GetValue());

    return ReturnType{
        allocator_->Allocate<ast::VarDeclStatement>(const_variable)};
  }

  if (Consume(TokenType::kLet)) {
    auto typed_ident = IdentWithOptionalTypeSpec(true);

    if (!Consume(TokenType::kEqual)) {
      diagnosis_.message = "Expected '=' after let variable name";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto init = Expression();

    if (init.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    auto& type_info = typed_ident.GetValue();

    auto let_variable = allocator_->Allocate<ast::LetVar>(
        type_info.name, type_info.type, init.GetValue());

    return ReturnType{
        allocator_->Allocate<ast::VarDeclStatement>(let_variable)};
  }

  auto decl = VariableDeclaration();

  if (decl.state != State::kSuccess) {
    return ReturnType{decl.state};
  }

  ast::Expression* initializer = nullptr;

  if (Consume(TokenType::kEqual)) {
    auto init = Expression();

    if (init.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    initializer = init.GetValue();
  }

  auto& decl_info = decl.GetValue();

  auto var = allocator_->Allocate<ast::Var>(
      decl_info.name, decl_info.type, decl_info.address_space, decl_info.access,
      initializer, std::vector<ast::Attribute*>{});

  return ReturnType{allocator_->Allocate<ast::VarDeclStatement>(var)};
}

Parser::Result<ast::CallStatement*> Parser::FuncCallStatement() {
  using ReturnType = Result<ast::CallStatement*>;

  const auto& t1 = Peek();
  const auto& t2 = Peek(1);

  if (!t1.Is(TokenType::kIdentifier) || !t2.Is(TokenType::kParenLeft)) {
    return ReturnType{State::kNotMatch};
  }

  // consume ident and `(`
  Advance(2);

  auto params = ExpressionList();

  if (params.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kParenRight)) {
    diagnosis_.message = "Expected ')' after function call params";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto name = allocator_->Allocate<ast::Identifier>(t1.ToString());
  std::vector<ast::Expression*> exps = std::move(params.GetValue());

  auto func_call = allocator_->Allocate<ast::FunctionCallExp>(
      allocator_->Allocate<ast::IdentifierExp>(name), std::move(exps));

  return ReturnType{allocator_->Allocate<ast::CallStatement>(func_call)};
}

Parser::Result<ast::ReturnStatement*> Parser::ReturnStatement() {
  using ReturnType = Result<ast::ReturnStatement*>;

  if (!Consume(TokenType::kReturn)) {
    return ReturnType{State::kNotMatch};
  }

  auto expr = Expression();
  if (expr.state == State::kError) {
    return ReturnType{State::kError};
  }

  if (expr.state == State::kSuccess) {
    return ReturnType{
        allocator_->Allocate<ast::ReturnStatement>(expr.GetValue())};
  } else {
    return ReturnType{allocator_->Allocate<ast::ReturnStatement>()};
  }
}

Parser::Result<ast::Statement*> Parser::NonBlockStatement() {
  using ReturnType = Result<ast::Statement*>;

  ast::Statement* stmt = nullptr;
  {
    auto ret_stmt = ReturnStatement();
    if (ret_stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (ret_stmt.state == State::kSuccess) {
      stmt = ret_stmt.GetValue();
    }
  }

  // try function call
  if (stmt == nullptr) {
    auto func_call = FuncCallStatement();
    if (func_call.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (func_call.state == State::kSuccess) {
      stmt = func_call.GetValue();
    }
  }
  // try variable statement
  if (stmt == nullptr) {
    auto var_stmt = VariableStatement();
    if (var_stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (var_stmt.state == State::kSuccess) {
      stmt = var_stmt.GetValue();
    }
  }
  // try break statement
  if (stmt == nullptr && Consume(TokenType::kBreak)) {
    stmt =
        allocator_->Allocate<ast::KeywordStatement>(ast::StatementType::kBreak);
  }
  // continue statement
  if (stmt == nullptr && Consume(TokenType::kContinue)) {
    stmt = allocator_->Allocate<ast::KeywordStatement>(
        ast::StatementType::kContinue);
  }
  // discard statement
  if (stmt == nullptr && Consume(TokenType::kDiscard)) {
    stmt = allocator_->Allocate<ast::KeywordStatement>(
        ast::StatementType::kDiscard);
  }

  if (stmt == nullptr) {
    auto assign = VariableUpdateStatement();

    if (assign.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (assign.state == State::kSuccess) {
      stmt = assign.GetValue();
    }
  }

  if (stmt == nullptr) {
    return ReturnType{State::kNotMatch};
  }

  if (!Consume(TokenType::kSemicolon)) {
    diagnosis_.message = "Expected ';' after statement";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  return ReturnType{stmt};
}

Parser::Result<Parser::IfInfo> Parser::ParseIf() {
  using ReturnType = Result<IfInfo>;

  if (!Consume(TokenType::kIf)) {
    return ReturnType{State::kNotMatch};
  }

  auto condition = Expression();

  if (condition.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto body = CompoundStatement();
  if (body.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  return ReturnType{IfInfo{
      condition.GetValue(),
      body.GetValue(),
  }};
}

Parser::Result<ast::IfStatement*> Parser::IfStatement(AttrList& attrs) {
  using ReturnType = Result<ast::IfStatement*>;

  std::vector<IfInfo> statements{};

  auto first_if = ParseIf();
  if (first_if.state == State::kError) {
    return ReturnType{State::kError};
  } else if (first_if.state == State::kNotMatch) {
    return ReturnType{State::kNotMatch};
  }

  statements.emplace_back(first_if.GetValue());
  statements.back().attributes = std::move(attrs);

  ast::Statement* last_stmt = nullptr;
  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    if (!Consume(TokenType::kElse)) {
      break;
    }

    auto else_if = ParseIf();
    if (else_if.state == State::kError) {
      return ReturnType{State::kError};
    } else if (else_if.state == State::kSuccess) {
      statements.emplace_back(else_if.GetValue());
      continue;
    }

    auto else_body = CompoundStatement();
    if (else_body.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    last_stmt = else_body.GetValue();
  }

  for (auto& itr : statements) {
    last_stmt = allocator_->Allocate<ast::IfStatement>(
        itr.condition, itr.body, last_stmt, std::move(itr.attributes));
  }

  return ReturnType{static_cast<ast::IfStatement*>(last_stmt)};
}

Parser::Result<ast::CaseSelector*> Parser::CaseSelector() {
  using ReturnType = Result<ast::CaseSelector*>;

  if (Consume(TokenType::kDefault)) {
    return ReturnType{allocator_->Allocate<ast::CaseSelector>()};
  }

  auto expr = Expression();

  if (expr.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  return ReturnType{allocator_->Allocate<ast::CaseSelector>(expr.GetValue())};
}

Parser::Result<CaseSelectorList> Parser::CaseSelectors() {
  using ReturnType = Result<CaseSelectorList>;

  CaseSelectorList selectors{};

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto expr = CaseSelector();

    if (expr.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (expr.state == State::kNotMatch) {
      break;
    }

    selectors.emplace_back(expr.GetValue());

    if (!Consume(TokenType::kComma)) {
      break;
    }
  }

  if (selectors.empty()) {
    diagnosis_.message = "Expected case selector";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  return ReturnType{std::move(selectors)};
}

Parser::Result<ast::CaseStatement*> Parser::SwitchBody() {
  using ReturnType = Result<ast::CaseStatement*>;

  if (!Peek().Is(TokenType::kCase) && !Peek().Is(TokenType::kDefault)) {
    return ReturnType{State::kNotMatch};
  }

  const auto& t = Peek();
  Advance();

  CaseSelectorList selector_list{};
  if (t.Is(TokenType::kCase)) {
    auto selectors = CaseSelectors();
    if (selectors.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    selector_list = std::move(selectors.GetValue());
  } else {
    auto selector = allocator_->Allocate<ast::CaseSelector>();
    selector_list.emplace_back(selector);
  }

  Consume(TokenType::kColon);  // optional `:`

  auto body = CompoundStatement();

  if (body.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto stmt = allocator_->Allocate<ast::CaseStatement>(std::move(selector_list),
                                                       body.GetValue());

  return ReturnType{stmt};
}

Parser::Result<ast::SwitchStatement*> Parser::SwitchStatement(AttrList& attrs) {
  using ReturnType = Result<ast::SwitchStatement*>;

  if (!Consume(TokenType::kSwitch)) {
    return ReturnType{State::kNotMatch};
  }

  auto condition = Expression();
  if (condition.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto body_attrs = AttributeList();

  if (body_attrs.state == State::kError) {
    return ReturnType{State::kError};
  }

  AttrList body_attr_list{};
  if (body_attrs.state == State::kSuccess) {
    body_attr_list = std::move(body_attrs.GetValue());
  }

  if (!Consume(TokenType::kBraceLeft)) {
    diagnosis_.message = "Expected '{' after switch statement";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  std::vector<ast::CaseStatement*> case_body{};

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto stmt = SwitchBody();
    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kNotMatch) {
      break;
    }

    case_body.emplace_back(stmt.GetValue());
  }

  if (!Consume(TokenType::kBraceRight)) {
    diagnosis_.message = "Expected '}' after switch statement";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto stmt = allocator_->Allocate<ast::SwitchStatement>(
      condition.GetValue(), std::move(case_body), std::move(attrs),
      std::move(body_attr_list));

  return ReturnType{stmt};
}

Parser::Result<ast::Statement*> Parser::BreakIfStatement() {
  using ReturnType = Result<ast::Statement*>;

  if (!Peek().Is(TokenType::kBreak) || !Peek(1).Is(TokenType::kIf)) {
    return ReturnType{State::kNotMatch};
  }

  Advance(2);

  auto expr = Expression();
  if (expr.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kSemicolon)) {
    diagnosis_.message = "Expected ';' after breakif expression";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  return ReturnType{
      allocator_->Allocate<ast::BreakIfStatement>(expr.GetValue())};
}

Parser::Result<ast::BlockStatement*> Parser::ContinuingCompoundStatement() {
  using ReturnType = Result<ast::BlockStatement*>;

  auto attrs = AttributeList();
  if (attrs.state == State::kError) {
    return ReturnType{State::kError};
  }

  AttrList attr_list{};
  if (attrs.state == State::kSuccess) {
    attr_list = std::move(attrs.GetValue());
  }

  if (!Consume(TokenType::kBraceLeft)) {
    diagnosis_.message = "Expected '{' here";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  StatementList stmts{};

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto break_if = BreakIfStatement();

    if (break_if.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (break_if.state == State::kSuccess) {
      stmts.emplace_back(break_if.GetValue());
      continue;
    }

    auto stmt = Statement();
    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kNotMatch) {
      break;
    }

    stmts.emplace_back(stmt.GetValue());
  }

  auto stmt = allocator_->Allocate<ast::BlockStatement>(std::move(stmts),
                                                        std::move(attr_list));

  return ReturnType{stmt};
}

Parser::Result<ast::BlockStatement*> Parser::ContinuingStatement() {
  using ReturnType = Result<ast::BlockStatement*>;

  if (!Consume(TokenType::kContinuing)) {
    return ReturnType{
        allocator_->Allocate<ast::BlockStatement>(StatementList{}, AttrList{})};
  }

  return ContinuingCompoundStatement();
}

Parser::Result<ast::LoopStatement*> Parser::LoopStatement(AttrList& attrs) {
  using ReturnType = Result<ast::LoopStatement*>;

  if (!Consume(TokenType::kLoop)) {
    return ReturnType{State::kNotMatch};
  }

  auto body_attrs = AttributeList();

  if (body_attrs.state == State::kError) {
    return ReturnType{State::kError};
  }

  AttrList body_attr_list{};
  if (body_attrs.state == State::kSuccess) {
    body_attr_list = std::move(body_attrs.GetValue());
  }

  if (!Consume(TokenType::kBraceLeft)) {
    diagnosis_.message = "Expected '{' after loop statement";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto stmts = Statements();
  if (stmts.state == State::kError) {
    return ReturnType{State::kError};
  }

  StatementList stmt_list{};
  if (stmts.state == State::kSuccess) {
    stmt_list = std::move(stmts.GetValue());
  }

  auto continuing = ContinuingStatement();
  if (continuing.state == State::kError) {
    return ReturnType{State::kError};
  }

  auto loop_stmt = allocator_->Allocate<ast::LoopStatement>(
      allocator_->Allocate<ast::BlockStatement>(std::move(stmt_list),
                                                std::move(body_attr_list)),
      continuing.GetValue(), std::move(attrs));

  return ReturnType{loop_stmt};
}

Parser::Result<ast::Statement*> Parser::ForHeaderContinuing() {
  using ReturnType = Result<ast::Statement*>;

  {
    auto call = FuncCallStatement();
    if (call.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (call.state == State::kSuccess) {
      return ReturnType{call.GetValue()};
    }
  }

  {
    auto assign = VariableUpdateStatement();
    if (assign.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (assign.state == State::kSuccess) {
      return ReturnType{assign.GetValue()};
    }
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<ast::Statement*> Parser::ForHeaderInitializer() {
  using ReturnType = Result<ast::Statement*>;

  {
    auto call = FuncCallStatement();
    if (call.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (call.state == State::kSuccess) {
      return ReturnType{call.GetValue()};
    }
  }

  {
    auto var = VariableStatement();
    if (var.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (var.state == State::kSuccess) {
      return ReturnType{var.GetValue()};
    }
  }

  {
    auto assign = VariableUpdateStatement();
    if (assign.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (assign.state == State::kSuccess) {
      return ReturnType{assign.GetValue()};
    }
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<Parser::ForHeader> Parser::ParseForHeader() {
  using ReturnType = Result<Parser::ForHeader>;

  ast::Statement* initializer = nullptr;
  {
    auto init = ForHeaderInitializer();
    if (init.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (init.state == State::kSuccess) {
      initializer = init.GetValue();
    }
  }

  if (!Consume(TokenType::kSemicolon)) {
    diagnosis_.message = "Expected ';' after initializer in for header";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  ast::Expression* condition = nullptr;
  {
    auto cond = Expression();
    if (cond.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (cond.state == State::kSuccess) {
      condition = cond.GetValue();
    }
  }

  if (!Consume(TokenType::kSemicolon)) {
    diagnosis_.message = "Expected ';' after condition in for header";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  ast::Statement* continuing = nullptr;
  {
    auto stmt = ForHeaderContinuing();
    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kSuccess) {
      continuing = stmt.GetValue();
    }
  }

  return ReturnType{ForHeader{
      initializer,
      condition,
      continuing,
  }};
}

Parser::Result<ast::ForLoopStatement*> Parser::ForStatement(AttrList& attrs) {
  using ReturnType = Result<ast::ForLoopStatement*>;

  if (!Consume(TokenType::kFor)) {
    return ReturnType{State::kNotMatch};
  }

  if (!Consume(TokenType::kParenLeft)) {
    diagnosis_.message = "Expected '(' after for statement";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto header = ParseForHeader();
  if (header.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kParenRight)) {
    diagnosis_.message = "Expected ')' after for header";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto body = CompoundStatement();
  if (body.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto stmt = allocator_->Allocate<ast::ForLoopStatement>(
      header.GetValue().initializer, header.GetValue().condition,
      header.GetValue().continuing, body.GetValue(), std::move(attrs));

  return ReturnType{stmt};
}

Parser::Result<ast::WhileLoopStatement*> Parser::WhileStatement(
    AttrList& attrs) {
  using ReturnType = Result<ast::WhileLoopStatement*>;

  if (!Consume(TokenType::kWhile)) {
    return ReturnType{State::kNotMatch};
  }

  auto condition = Expression();
  if (condition.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto body = CompoundStatement();

  if (body.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto stmt = allocator_->Allocate<ast::WhileLoopStatement>(
      condition.GetValue(), body.GetValue(), std::move(attrs));

  return ReturnType{stmt};
}

Parser::Result<ast::Statement*> Parser::Statement() {
  using ReturnType = Result<ast::Statement*>;

  while (Consume(TokenType::kSemicolon)) {
    // skip empty statements
  }

  auto attrs = AttributeList();

  if (attrs.state == State::kError) {
    return ReturnType{State::kError};
  }

  std::vector<ast::Attribute*> attribute_list{};
  if (attrs.state == State::kSuccess) {
    attribute_list = std::move(attrs.GetValue());
  }

  {
    auto stmt = NonBlockStatement();
    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kSuccess) {
      if (!attribute_list.empty()) {
        diagnosis_.message = "Attributes are not allowed here";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return ReturnType{State::kError};
      }

      return stmt;
    }
  }

  {
    auto stmt = IfStatement(attribute_list);

    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kSuccess) {
      return ReturnType{stmt.GetValue()};
    }
  }

  {
    auto stmt = SwitchStatement(attribute_list);

    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kSuccess) {
      return ReturnType{stmt.GetValue()};
    }
  }

  {
    auto stmt = LoopStatement(attribute_list);

    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kSuccess) {
      return ReturnType{stmt.GetValue()};
    }
  }

  {
    auto stmt = ForStatement(attribute_list);

    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kSuccess) {
      return ReturnType{stmt.GetValue()};
    }
  }

  {
    auto stmt = WhileStatement(attribute_list);

    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (stmt.state == State::kSuccess) {
      return ReturnType{stmt.GetValue()};
    }
  }

  if (Peek().Is(TokenType::kBraceLeft)) {
    auto body = CompoundStatement(attribute_list);

    if (body.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    return ReturnType{body.GetValue()};
  }

  if (!attribute_list.empty()) {
    diagnosis_.message = "Attributes are not allowed here";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<StatementList> Parser::Statements() {
  using ReturnType = Result<StatementList>;

  StatementList stmts{};

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto stmt = Statement();
    if (stmt.state == State::kError) {
      return ReturnType{State::kError};
    } else if (stmt.state == State::kNotMatch) {
      break;
    } else {
      stmts.emplace_back(stmt.GetValue());
    }
  }

  if (stmts.empty()) {
    return ReturnType{State::kNotMatch};
  }

  return ReturnType{std::move(stmts)};
}

Parser::Result<ast::BlockStatement*> Parser::CompoundStatement(
    AttrList& attrs) {
  using ReturnType = Result<ast::BlockStatement*>;

  if (!Consume(TokenType::kBraceLeft)) {
    return ReturnType{State::kNotMatch};
  }

  auto stmts = Statements();

  if (stmts.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kBraceRight)) {
    return ReturnType{State::kSuccess};
  }

  auto bs = allocator_->Allocate<ast::BlockStatement>(
      std::move(stmts.GetValue()), std::move(attrs));

  return ReturnType{bs};
}

Parser::Result<ast::BlockStatement*> Parser::CompoundStatement() {
  using ReturnType = Result<ast::BlockStatement*>;

  auto attr_list = AttributeList();

  if (attr_list.state == State::kError) {
    return ReturnType{State::kError};
  }

  AttrList attrs{};
  if (attr_list.state == State::kSuccess) {
    attrs = std::move(attr_list.GetValue());
  }

  return CompoundStatement(attrs);
}

Parser::Result<ast::Function*> Parser::FunctionDeclaration(
    std::vector<ast::Attribute*>& attrs) {
  using ReturnType = Result<ast::Function*>;

  auto header = FunctionHeaderDecl();

  if (header.state != State::kSuccess) {
    return ReturnType{header.state};
  }

  auto body = CompoundStatement();

  if (body.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto& header_info = header.GetValue();

  auto func = allocator_->Allocate<ast::Function>(
      header_info.name, std::move(header_info.params), header_info.return_type,
      body.GetValue(), std::move(attrs),
      std::move(header_info.return_type_attrs));

  return ReturnType{func};
}

Parser::Result<Parser::VarQualifier> Parser::VariableQualifier() {
  using ReturnType = Result<VarQualifier>;

  if (!Consume(TokenType::kTemplateArgsLeft)) {
    return ReturnType{State::kNotMatch};
  }

  auto address_space = Expression();

  ast::Expression* access = nullptr;

  if (address_space.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (Consume(TokenType::kComma)) {
    auto acc = Expression();
    if (acc.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    access = acc.GetValue();
  }

  if (!Consume(TokenType::kTemplateArgsRight)) {
    diagnosis_.message = "Expected '>' after variable qualifier";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  return ReturnType{VarQualifier{
      address_space.GetValue(),
      access,
  }};
}

Parser::Result<ast::Identifier*> Parser::Identifier() {
  using ReturnType = Result<ast::Identifier*>;

  const auto& t = Peek();

  if (!Consume(TokenType::kIdentifier)) {
    diagnosis_.message = "Expected identifier";
    diagnosis_.line = t.line;
    diagnosis_.column = t.column;
    return ReturnType{State::kError};
  }

  auto id = allocator_->Allocate<ast::Identifier>(t.ToString());

  return ReturnType{id};
}

Parser::Result<ast::Type> Parser::TypeSpecifier() {
  using ReturnType = Result<ast::Type>;

  auto const& ident = Peek();

  if (!Consume(TokenType::kIdentifier)) {
    diagnosis_.message = "Expected identifier";
    diagnosis_.line = ident.line;
    diagnosis_.column = ident.column;
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kTemplateArgsLeft)) {
    auto id = allocator_->Allocate<ast::Identifier>(ident.ToString());
    ast::Type type{allocator_->Allocate<ast::IdentifierExp>(id)};

    return ReturnType{type};
  }

  auto args = ExpressionList();

  if (args.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (!Consume(TokenType::kTemplateArgsRight)) {
    diagnosis_.message = "Expected '>' after type specifier";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto id = allocator_->Allocate<ast::Identifier>(
      ident.ToString(), args.GetValue(), std::vector<ast::Attribute*>{});

  ast::Type type{allocator_->Allocate<ast::IdentifierExp>(id)};

  return ReturnType{type};
}

Parser::Result<Parser::TypeIdentifier> Parser::IdentWithOptionalTypeSpec(
    bool allow_inferred) {
  using ReturnType = Result<TypeIdentifier>;

  auto ident = Identifier();

  if (ident.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  if (allow_inferred && !Peek().Is(TokenType::kColon)) {
    // WGSL allow inferred type for variable declaration.
    // But we don't support it now. So mark it as error.
    diagnosis_.message = "Inferred type is not supported";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};

    // return ReturnType{TypeIdentifier{
    //     ast::Type{},
    //     ident.GetValue(),
    // }};
  }

  if (!Consume(TokenType::kColon)) {
    diagnosis_.message = "Expected type annotation after identifier";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  auto type = TypeSpecifier();

  if (type.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  return ReturnType{TypeIdentifier{
      type.GetValue(),
      ident.GetValue(),
  }};
}

Parser::Result<Parser::VarDeclInfo> Parser::VariableDeclaration() {
  using ReturnType = Result<VarDeclInfo>;

  if (!Consume(TokenType::kVar)) {
    return ReturnType{State::kNotMatch};
  }

  VarQualifier vq{};

  auto explicit_vq = VariableQualifier();
  if (explicit_vq.state == State::kError) {
    return ReturnType{State::kError};
  }

  if (explicit_vq.state == State::kSuccess) {
    vq = explicit_vq.GetValue();
  }

  auto decl = IdentWithOptionalTypeSpec(true);

  if (decl.state == State::kError) {
    return ReturnType{State::kError};
  }

  return ReturnType{VarDeclInfo{decl.GetValue().name, vq.address_space,
                                vq.access, decl.GetValue().type}};
}

Parser::Result<AttrList> Parser::AttributeList() {
  using ReturnType = Result<AttrList>;

  AttrList attrs{};

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    if (Peek().Is(TokenType::kAttr)) {
      auto attr = Attribute();

      if (attr.state != State::kSuccess) {
        has_error_ = true;
        return ReturnType{State::kError};
      } else {
        attrs.emplace_back(attr.GetValue());
      }

    } else {
      break;
    }
  }

  if (attrs.empty()) {
    return ReturnType{State::kNotMatch};
  } else {
    return ReturnType{attrs};
  }
}

Parser::Result<ast::Attribute*> Parser::Attribute() {
  using ReturnType = Result<ast::Attribute*>;

  if (!Consume(TokenType::kAttr)) {  // '@'
    return ReturnType{State::kNotMatch};
  }

  // check simple attribute
  if (Consume(TokenType::kConst)) {
    if (Peek().Is(TokenType::kParenLeft)) {
      diagnosis_.message = "UnExpected '(' after const attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::NamedAttribute>(
        "const", ast::AttributeType::kConst)};
  } else if (Consume(TokenType::kIdentifier, "invariant")) {
    if (Peek().Is(TokenType::kParenLeft)) {
      diagnosis_.message = "UnExpected '(' after invariant attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::NamedAttribute>(
        "invariant", ast::AttributeType::kInvariant)};
  } else if (Consume(TokenType::kIdentifier, "must_use")) {
    if (Peek().Is(TokenType::kParenLeft)) {
      diagnosis_.message = "UnExpected '(' after must_use attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::NamedAttribute>(
        "must_use", ast::AttributeType::kMustUse)};
  } else if (Consume(TokenType::kIdentifier, "vertex")) {
    if (Peek().Is(TokenType::kParenLeft)) {
      diagnosis_.message = "UnExpected '(' after vertex attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::NamedAttribute>(
        "vertex", ast::AttributeType::kVertex)};
  } else if (Consume(TokenType::kIdentifier, "fragment")) {
    if (Peek().Is(TokenType::kParenLeft)) {
      diagnosis_.message = "UnExpected '(' after fragment attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::NamedAttribute>(
        "fragment", ast::AttributeType::kFragment)};
  } else if (Consume(TokenType::kIdentifier, "compute")) {
    if (Peek().Is(TokenType::kParenLeft)) {
      diagnosis_.message = "UnExpected '(' after compute attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::NamedAttribute>(
        "compute", ast::AttributeType::kCompute)};
  }

  if (Consume(TokenType::kIdentifier, "align")) {  // align
    if (!Consume(TokenType::kParenLeft)) {
      diagnosis_.message = "Expected '(' after align attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto exp = ConstLiteral();
    if (exp.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    if (!Consume(TokenType::kParenRight)) {
      diagnosis_.message = "Expected ')' after align attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto const_exp = exp.GetValue();
    if (const_exp->GetType() != ast::ExpressionType::kIntLiteral) {
      // Note: WGSL support const expression like "align(4 + 4)".
      // But we don't support it now. So mark it as error.
      diagnosis_.message = "Expected integer literal after align attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto offset = static_cast<ast::IntLiteralExp*>(const_exp)->value;

    if (offset < 0) {
      diagnosis_.message =
          "Expected positive integer literal after align attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::AlignAttribute>(offset)};
  }

  if (Consume(TokenType::kIdentifier, "binding")) {
    if (!Consume(TokenType::kParenLeft)) {
      diagnosis_.message = "Expected '(' after binding attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto exp = ConstLiteral();
    if (exp.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    if (!Consume(TokenType::kParenRight)) {
      diagnosis_.message = "Expected ')' after binding attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto const_exp = exp.GetValue();
    if (const_exp->GetType() != ast::ExpressionType::kIntLiteral) {
      // Note: WGSL support const expression like "binding(4 + 4)".
      // But we don't support it now. So mark it as error.
      diagnosis_.message = "Expected integer literal after binding attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto index = static_cast<ast::IntLiteralExp*>(const_exp)->value;

    if (index < 0) {
      diagnosis_.message =
          "Expected positive integer literal after binding attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::BindingAttribute>(index)};
  }

  if (Consume(TokenType::kIdentifier, "builtin")) {
    if (!Consume(TokenType::kParenLeft)) {
      diagnosis_.message = "Expected '(' after builtin attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    ast::BuiltinAttribute* built_in_attr = nullptr;

    if (Consume(TokenType::kIdentifier, "position")) {
      built_in_attr = allocator_->Allocate<ast::BuiltinAttribute>("position");
    } else if (Consume(TokenType::kIdentifier, "vertex_index")) {
      built_in_attr =
          allocator_->Allocate<ast::BuiltinAttribute>("vertex_index");
    } else if (Consume(TokenType::kIdentifier, "instance_index")) {
      built_in_attr =
          allocator_->Allocate<ast::BuiltinAttribute>("instance_index");
    } else {
      diagnosis_.message = "Unsupported builtin attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    if (!Consume(TokenType::kParenRight)) {
      diagnosis_.message = "Expected ')' after builtin attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{built_in_attr};
  }

  if (Consume(TokenType::kIdentifier, "group")) {
    if (!Consume(TokenType::kParenLeft)) {
      diagnosis_.message = "Expected '(' after group attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto exp = ConstLiteral();
    if (exp.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    if (!Consume(TokenType::kParenRight)) {
      diagnosis_.message = "Expected ')' after group attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto const_exp = exp.GetValue();
    if (const_exp->GetType() != ast::ExpressionType::kIntLiteral) {
      // Note: WGSL support const expression like "group(4 + 4)".
      // But we don't support it now. So mark it as error.
      diagnosis_.message = "Expected integer literal after group attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto index = static_cast<ast::IntLiteralExp*>(const_exp)->value;

    if (index < 0) {
      diagnosis_.message =
          "Expected positive integer literal after group attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::GroupAttribute>(index)};
  }

  if (Consume(TokenType::kIdentifier, "location")) {
    if (!Consume(TokenType::kParenLeft)) {
      diagnosis_.message = "Expected '(' after location attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto exp = ConstLiteral();
    if (exp.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    if (!Consume(TokenType::kParenRight)) {
      diagnosis_.message = "Expected ')' after location attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto const_exp = exp.GetValue();
    if (const_exp->GetType() != ast::ExpressionType::kIntLiteral) {
      // Note: WGSL support const expression like "location(4 + 4)".
      // But we don't support it now. So mark it as error.
      diagnosis_.message = "Expected integer literal after location attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto index = static_cast<ast::IntLiteralExp*>(const_exp)->value;

    if (index < 0) {
      diagnosis_.message =
          "Expected positive integer literal after location attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::LocationAttribute>(index)};
  }

  if (Consume(TokenType::kIdentifier, "interpolate")) {
    if (!Consume(TokenType::kParenLeft)) {
      diagnosis_.message = "Expected '(' after interpolate attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto type_str = Peek();

    if (!Consume(TokenType::kIdentifier)) {
      diagnosis_.message =
          "Expected interpolate type after interpolate attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    auto type = ast::InterpolateAttribute::ParseType(type_str.ToString());

    ast::InterpolateSampling sampling = ast::InterpolateSampling::kUndefined;

    if (Consume(TokenType::kComma)) {
      auto sampling_str = Peek();
      if (!Consume(TokenType::kIdentifier)) {
        return ReturnType{State::kError};
      }

      sampling =
          ast::InterpolateAttribute::ParseSampling(sampling_str.ToString());
    }

    if (!Consume(TokenType::kParenRight)) {
      diagnosis_.message = "Expected ')' after interpolate attribute";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{
        allocator_->Allocate<ast::InterpolateAttribute>(type, sampling)};
  }

  // other attribute not support for now
  diagnosis_.message = "Unknown attribute";
  diagnosis_.line = Peek().line;
  diagnosis_.column = Peek().column;
  return ReturnType{State::kError};
}

Parser::Result<ast::Expression*> Parser::ConstLiteral() {
  using ReturnType = Result<ast::Expression*>;

  const auto& token = Peek();
  if (Consume(TokenType::kIntLiteral)) {
    return ReturnType{allocator_->Allocate<ast::IntLiteralExp>(token.ToI64())};
  }

  if (Consume(TokenType::kFloatLiteral)) {
    return ReturnType{
        allocator_->Allocate<ast::FloatLiteralExp>(token.ToF64())};
  }

  if (Consume(TokenType::kTrue)) {
    return ReturnType{allocator_->Allocate<ast::BoolLiteralExp>(true)};
  }

  if (Consume(TokenType::kFalse)) {
    return ReturnType{allocator_->Allocate<ast::BoolLiteralExp>(false)};
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<ast::Expression*> Parser::PrimaryExpression() {
  using ReturnType = Result<ast::Expression*>;

  auto literal = ConstLiteral();
  if (literal.state == State::kSuccess) {
    return literal;
  }

  const auto& token = Peek();
  if (token.Is(TokenType::kIdentifier)) {
    Advance();

    ast::Identifier* ident = nullptr;

    // begin template args
    if (Consume(TokenType::kTemplateArgsLeft)) {  // <
      auto args = ExpressionList();
      if (args.state != State::kSuccess) {
        // no template args but has a template begin symbol this is error
        return ReturnType{State::kError};
      }

      auto& arg_list = args.GetValue();
      if (arg_list.empty()) {
        diagnosis_.message = "Empty template args";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return ReturnType{State::kError};
      }

      if (!Consume(TokenType::kTemplateArgsRight)) {
        diagnosis_.message = "Missing template args end symbol";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return ReturnType{State::kError};
      }

      ident = allocator_->Allocate<ast::Identifier>(
          token.ToString(), arg_list, std::vector<ast::Attribute*>{});
    } else {
      // only has name
      ident = allocator_->Allocate<ast::Identifier>(token.ToString());
    }

    if (Consume(TokenType::kParenLeft)) {  // (
      // identifier with `(` means this is a function call
      auto params = ExpressionList();
      if (params.state == State::kError) {
        return ReturnType{State::kError};
      }

      if (!Consume(TokenType::kParenRight)) {  // )
        // no `)` means error
        diagnosis_.message = "Missing function call end symbol ')' ";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return ReturnType{State::kError};
      }

      auto id_exp = allocator_->Allocate<ast::IdentifierExp>(ident);

      auto param_list = params.state == State::kSuccess
                            ? params.GetValue()
                            : std::vector<ast::Expression*>{};

      return ReturnType{
          allocator_->Allocate<ast::FunctionCallExp>(id_exp, param_list)};
    }

    return ReturnType{allocator_->Allocate<ast::IdentifierExp>(ident)};
  }

  if (Consume(TokenType::kParenLeft)) {  // (
    auto params = ExpressionList();

    // () is error for this parser
    if (params.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    if (!Consume(TokenType::kParenRight)) {  // )
      diagnosis_.message = "Expected ')' here";
      diagnosis_.line = Peek().line;
      diagnosis_.column = Peek().column;
      return ReturnType{State::kError};
    }

    return ReturnType{allocator_->Allocate<ast::ParenExp>(params.GetValue())};
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<ast::Expression*> Parser::ComponentOrSwizzle(
    ast::Expression* prefix) {
  using ReturnType = Result<ast::Expression*>;

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    if (Consume(TokenType::kBracketLeft)) {  // [
      auto param = Expression();
      if (param.state != State::kSuccess) {
        return ReturnType{param.state};
      }

      if (!Consume(TokenType::kBracketRight)) {  // ]
        // no ] after expression
        diagnosis_.message = "Missing ']' after expression";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return ReturnType{State::kError};
      }

      prefix =
          allocator_->Allocate<ast::IndexAccessorExp>(prefix, param.GetValue());

      continue;
    }

    if (Consume(TokenType::kPeriod)) {  // .
      if (!Peek().Is(TokenType::kIdentifier)) {
        diagnosis_.message = "Expected identifier after '.'";
        diagnosis_.line = Peek().line;
        diagnosis_.column = Peek().column;
        return ReturnType{State::kError};
      }

      const auto& id = Peek();
      Advance();

      prefix = allocator_->Allocate<ast::MemberAccessor>(
          prefix, allocator_->Allocate<ast::Identifier>(id.ToString()));

      continue;
    }

    return ReturnType{prefix};
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<ast::Expression*> Parser::SingularExpression() {
  using ReturnType = Result<ast::Expression*>;

  auto prefix = PrimaryExpression();

  if (prefix.state != State::kSuccess) {
    return ReturnType{prefix.state};
  }

  return ComponentOrSwizzle(prefix.GetValue());
}

Parser::Result<ast::Expression*> Parser::UnaryExpression() {
  using ReturnType = Result<ast::Expression*>;

  if (Peek().Is(TokenType::kPlusPlus) || Peek().Is(TokenType::kMinusMinus)) {
    // wgsl reserved this feature
    diagnosis_.message = "Unary operator not support";
    diagnosis_.line = Peek().line;
    diagnosis_.column = Peek().column;
    return ReturnType{State::kError};
  }

  ast::UnaryOp op;
  if (Consume(TokenType::kMinus)) {
    op = ast::UnaryOp::kNegation;
  } else if (Consume(TokenType::kBang)) {
    op = ast::UnaryOp::kNot;
  } else if (Consume(TokenType::kTilde)) {
    op = ast::UnaryOp::kComplement;
  } else if (Consume(TokenType::kStar)) {
    op = ast::UnaryOp::kIndirection;
  } else if (Consume(TokenType::kAnd)) {
    op = ast::UnaryOp::kAddressOf;
  } else {
    return SingularExpression();
  }

  auto exp = UnaryExpression();

  if (exp.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  return ReturnType{allocator_->Allocate<ast::UnaryExp>(op, exp.GetValue())};
}

Parser::Result<ast::Expression*> Parser::AdditiveExpPostUnaryExpr(
    ast::Expression* lhs) {
  using ReturnType = Result<ast::Expression*>;

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto op = AdditiveOp();

    if (op.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (op.state == State::kNotMatch) {
      return ReturnType{lhs};
    }

    auto unary = UnaryExpression();

    if (unary.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    auto rhs = MultiplyExpPostUnaryExpr(unary.GetValue());

    if (rhs.state == State::kError) {
      return ReturnType{State::kError};
    }

    lhs = allocator_->Allocate<ast::BinaryExp>(op.GetValue(), lhs,
                                               rhs.GetValue());
  }

  return ReturnType{State::kError};
}

Parser::Result<ast::Expression*> Parser::MultiplyExpPostUnaryExpr(
    ast::Expression* lhs) {
  using ReturnType = Result<ast::Expression*>;

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto op = MultiplicativeOp();

    if (op.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (op.state == State::kNotMatch) {
      return ReturnType{lhs};
    }

    auto rhs = UnaryExpression();
    if (rhs.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    lhs = allocator_->Allocate<ast::BinaryExp>(op.GetValue(), lhs,
                                               rhs.GetValue());
  }

  return ReturnType{State::kError};
}

Parser::Result<ast::Expression*> Parser::MathExpPostUnaryExpr(
    ast::Expression* lhs) {
  using ReturnType = Result<ast::Expression*>;

  auto rhs = MultiplyExpPostUnaryExpr(lhs);

  if (rhs.state == State::kError) {
    return ReturnType{State::kError};
  }

  return AdditiveExpPostUnaryExpr(rhs.GetValue());
}

Parser::Result<ast::BinaryOp> Parser::AdditiveOp() {
  using ReturnType = Result<ast::BinaryOp>;

  if (Consume(TokenType::kPlus)) {
    return ReturnType{ast::BinaryOp::kAdd};
  }

  if (Consume(TokenType::kMinus)) {
    return ReturnType{ast::BinaryOp::kSubtract};
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<ast::BinaryOp> Parser::MultiplicativeOp() {
  using ReturnType = Result<ast::BinaryOp>;

  if (Consume(TokenType::kForwardSlash)) {
    return ReturnType{ast::BinaryOp::kDivide};
  }

  if (Consume(TokenType::kMod)) {
    return ReturnType{ast::BinaryOp::kModulo};
  }

  if (Consume(TokenType::kStar)) {
    return ReturnType{ast::BinaryOp::kMultiply};
  }

  return ReturnType{State::kNotMatch};
}

Parser::Result<ast::Expression*> Parser::BitwiseExpPostUnaryExpr(
    ast::Expression* lhs) {
  using ReturnType = Result<ast::Expression*>;

  std::optional<ast::BinaryOp> op{};
  TokenType type = TokenType::kError;
  if (Consume(TokenType::kAnd)) {
    type = TokenType::kAnd;
    op = ast::BinaryOp::kAnd;
  } else if (Consume(TokenType::kOr)) {
    type = TokenType::kOr;
    op = ast::BinaryOp::kOr;
  } else if (Consume(TokenType::kXor)) {
    type = TokenType::kXor;
    op = ast::BinaryOp::kXor;
  } else {
    return ReturnType{State::kNotMatch};
  }

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto rhs = UnaryExpression();

    if (rhs.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    lhs = allocator_->Allocate<ast::BinaryExp>(*op, lhs, rhs.GetValue());

    if (!Consume(type)) {
      return ReturnType{lhs};
    }
  }

  return ReturnType{State::kError};
}

Parser::Result<ast::Expression*> Parser::ShiftExpression() {
  using ReturnType = Result<ast::Expression*>;

  auto lhs = UnaryExpression();

  if (lhs.state != State::kSuccess) {
    return ReturnType{lhs.state};
  }

  return ShiftExpPostUnaryExpr(lhs.GetValue());
}

Parser::Result<ast::Expression*> Parser::RelationalExpression() {
  using ReturnType = Result<ast::Expression*>;

  auto lhs = UnaryExpression();

  if (lhs.state != State::kSuccess) {
    return ReturnType{lhs.state};
  }

  return RelationExpPostUnaryExpr(lhs.GetValue());
}

Parser::Result<ast::Expression*> Parser::ShiftExpPostUnaryExpr(
    ast::Expression* lhs) {
  using ReturnType = Result<ast::Expression*>;

  const auto& t = Peek();

  if (Consume(TokenType::kShiftLeft) || Consume(TokenType::kShiftRight)) {
    std::optional<ast::BinaryOp> op{};

    if (t.Is(TokenType::kShiftLeft)) {
      op = ast::BinaryOp::kShiftLeft;
    } else {
      op = ast::BinaryOp::kShiftRight;
    }

    auto rhs = UnaryExpression();

    if (rhs.state != State::kSuccess) {
      return ReturnType{State::kError};
    }

    return ReturnType{
        allocator_->Allocate<ast::BinaryExp>(*op, lhs, rhs.GetValue())};
  }

  return MathExpPostUnaryExpr(lhs);
}

Parser::Result<ast::Expression*> Parser::RelationExpPostUnaryExpr(
    ast::Expression* lhs) {
  using ReturnType = Result<ast::Expression*>;

  auto lhs_result = ShiftExpPostUnaryExpr(lhs);

  if (lhs_result.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  lhs = lhs_result.GetValue();

  std::optional<ast::BinaryOp> op{};
  TokenType type;

  if (Consume(TokenType::kLessThan)) {
    type = TokenType::kLessThan;
    op = ast::BinaryOp::kLessThan;
  } else if (Consume(TokenType::kGreaterThan)) {
    type = TokenType::kGreaterThan;
    op = ast::BinaryOp::kGreaterThan;
  } else if (Consume(TokenType::kLessThanEqual)) {
    type = TokenType::kLessThanEqual;
    op = ast::BinaryOp::kLessThanEqual;
  } else if (Consume(TokenType::kGreaterThanEqual)) {
    type = TokenType::kGreaterThanEqual;
    op = ast::BinaryOp::kGreaterThanEqual;
  } else if (Consume(TokenType::kEqualEqual)) {
    type = TokenType::kEqualEqual;
    op = ast::BinaryOp::kEqual;
  } else if (Consume(TokenType::kNotEqual)) {
    type = TokenType::kNotEqual;
    op = ast::BinaryOp::kNotEqual;
  } else {
    return ReturnType{lhs};
  }

  auto rhs = ShiftExpression();

  if (rhs.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  return ReturnType{
      allocator_->Allocate<ast::BinaryExp>(*op, lhs, rhs.GetValue())};
}

Parser::Result<ast::Expression*> Parser::Expression() {
  using ReturnType = Result<ast::Expression*>;

  auto lhs = UnaryExpression();
  if (lhs.state != State::kSuccess) {
    return ReturnType{lhs.state};
  }

  auto bitwise = BitwiseExpPostUnaryExpr(lhs.GetValue());
  if (bitwise.state == State::kError) {
    return ReturnType{State::kError};
  }

  if (bitwise.state == State::kSuccess) {
    return bitwise;
  }

  auto relational = RelationExpPostUnaryExpr(lhs.GetValue());

  if (relational.state != State::kSuccess) {
    return ReturnType{State::kError};
  }

  auto ret = relational.GetValue();

  const auto& t = Peek();
  if (t.Is(TokenType::kAndAnd) || t.Is(TokenType::kOrOr)) {
    ast::BinaryOp op = t.Is(TokenType::kAndAnd) ? ast::BinaryOp::kLogicalAnd
                                                : ast::BinaryOp::kLogicalOr;

    while (!Peek().Is(TokenType::kEOF) && !has_error_) {
      if (!Consume(t.type)) {
        break;
      }

      auto rhs = RelationalExpression();

      if (rhs.state != State::kSuccess) {
        return ReturnType{State::kError};
      }

      ret = allocator_->Allocate<ast::BinaryExp>(op, ret, rhs.GetValue());
    }
  }

  return ReturnType{ret};
}

Parser::Result<std::vector<ast::Expression*>> Parser::ExpressionList() {
  using ReturnType = Result<std::vector<ast::Expression*>>;

  std::vector<ast::Expression*> exprs{};

  while (!Peek().Is(TokenType::kEOF) && !has_error_) {
    auto exp = Expression();

    if (exp.state == State::kError) {
      return ReturnType{State::kError};
    }

    if (exp.state == State::kNotMatch) {
      break;
    }

    exprs.emplace_back(exp.GetValue());

    if (!Consume(TokenType::kComma)) {
      // no other expression in next
      break;
    }
  }

  return ReturnType{exprs};
}

}  // namespace wgx
