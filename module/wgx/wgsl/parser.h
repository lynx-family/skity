// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <optional>
#include <vector>

#include "wgsl/ast/attribute.h"
#include "wgsl/ast/expression.h"
#include "wgsl/ast/function.h"
#include "wgsl/ast/module.h"
#include "wgsl/ast/node.h"
#include "wgsl/ast/statement.h"
#include "wgsl/ast/type.h"
#include "wgsl/ast/type_decl.h"
#include "wgsl/ast/variable.h"
#include "wgsl/token.h"
#include "wgsl_cross.h"

namespace wgx {

using AttrList = std::vector<ast::Attribute*>;
using StructMemberList = std::vector<ast::StructMember*>;
using ParameterList = std::vector<ast::Parameter*>;
using StatementList = std::vector<ast::Statement*>;
using CaseSelectorList = std::vector<ast::CaseSelector*>;

/// recursive-descent parser based on
/// https://www.w3.org/TR/WGSL/#grammar-recursive-descent inspired by tint
class Parser {
  enum class State {
    kSuccess,
    kNotMatch,
    kError,
  };

  template <typename T>
  struct Result {
    using ValueT = T;

    State state;
    std::optional<T> value;

    explicit Result(ValueT v) : state(State::kSuccess), value(v) {}

    explicit Result(State state) : state(state), value() {}

    T& GetValue() { return *value; }

    const T& GetValue() const { return *value; }
  };

  struct VarDeclInfo {
    ast::Identifier* name = nullptr;
    ast::Expression* address_space = nullptr;
    ast::Expression* access = nullptr;
    ast::Type type = {};
  };

  struct VarQualifier {
    ast::Expression* address_space = nullptr;
    ast::Expression* access = nullptr;
  };

  struct TypeIdentifier {
    ast::Type type = {};
    ast::Identifier* name = nullptr;
  };

  struct FunctionHeader {
    ast::Identifier* name = nullptr;
    std::vector<ast::Parameter*> params = {};
    ast::Type return_type = {};
    AttrList return_type_attrs = {};
  };

  struct ForHeader {
    ast::Statement* initializer = nullptr;
    ast::Expression* condition = nullptr;
    ast::Statement* continuing = nullptr;
  };

  struct IfInfo {
    ast::Expression* condition = {};
    ast::BlockStatement* body = {};
    AttrList attributes = {};
  };

 public:
  explicit Parser(ast::NodeAllocator* allocator,
                  const std::vector<Token>& tokens)
      : allocator_(allocator), tokens_(tokens) {}

  ~Parser() = default;

  ast::Module* BuildModule();

  Diagnosis GetDiagnosis() const { return diagnosis_; }

 private:
  const Token& Peek(size_t offset = 0);

  bool Consume(TokenType type);

  bool Consume(TokenType type, std::string_view content);

  void Advance(size_t offset = 1);

  /**
   * translation_unit:
   *    global_decl *
   *
   * @note:
   *    not support global_directive, since this implementation only focus on
   * basic shader logical
   */
  void TranslationUnit();

  /**
   * global_decl:
   *   SEMICOLON
   *   | global_variable_decl SEMICOLON
   *   | global_constant_decl SEMICOLON
   *   | type_alias_decl SEMICOLON
   *   | struct_decl
   *   | function_decl
   *   | const_assert_statement SEMICOLON
   */
  State GlobalDecl();

  /**
   * global_variable_decl:
   *    variable_attribute_list* variable_decl ( `=` expression)?
   */
  Result<ast::Variable*> GlobalVariableDecl(
      std::vector<ast::Attribute*>& attrs);

  /**
   * global_constant_decl:
   *    `const` optionally_typed_ident `=` expression
   *
   * @note the override syntax is not support
   */
  Result<ast::Variable*> GlobalConstDecl(std::vector<ast::Attribute*>& attrs);

  /**
   * type_alias_decl:
   *    `alias` IDENT `=` type_specifier
   */
  Result<ast::Alias*> TypeAliasDecl();

  /**
   * struct_member:
   *    attribute* ident_with_type_specifier
   */
  Result<ast::StructMember*> StructMemberDecl();

  /**
   * struct_body_decl:
   *    `{` ( struct_member `,` )* struct_member `,`? `}`
   */
  Result<StructMemberList> StructBodyDecl();

  /**
   * struct_decl:
   *    `struct` IDENT struct_body_decl
   */
  Result<ast::StructDecl*> StructDeclaration();

  /**
   * param:
   *    attribute_list* ident `:` type_specifier
   */
  Result<ast::Parameter*> Parameter();

  /**
   * param_list:
   *   empty
   *   (param ',')* param ','?
   */
  Result<ParameterList> ParamList();

  /**
   * function_header:
   *    `fn` IDENT `(` param_list `)` return_type_specifier_optional
   *
   * return_type_specifier_optional:
   *    `->` attribute_list* type_specifier
   */
  Result<FunctionHeader> FunctionHeaderDecl();

  /**
   * compound_assignment_operator:
   *    plus_equal
   *    minus_equal
   *    times_equal
   *    division_equal
   *    modulo_equal
   *    and_equal
   *    or_equal
   *    xor_equal
   *    shift_right_equal
   *    shift_left_equal
   */
  Result<ast::BinaryOp> CompoundAssignmentOperator();

  /**
   * variable_updating_statement:
   *    lhs_expression (EQUAL | compound_assignment_operator ) expression
   *    lhs_expression MINUS_MINUS
   *    lhs_expression PLUS_PLUS
   *    UNDERSCORE EQUAL expression
   */
  Result<ast::Statement*> VariableUpdateStatement();

  /**
   * variable_statement:
   *    variable_decl
   *    variable_decl `=` expression
   *    `let` optionally_typed_ident `=` expression
   *    `const` optionally_typed_ident `=` expression
   */
  Result<ast::VarDeclStatement*> VariableStatement();

  /**
   * func_call_statement:
   *    IDENT argument_expression_list
   */
  Result<ast::CallStatement*> FuncCallStatement();

  /**
   * return_statement:
   *    `return` expression?
   */
  Result<ast::ReturnStatement*> ReturnStatement();

  /**
   * non_block_statement:
   *    return_statement `;`
   *    func_call_statement `;`
   *    variable_statement `;`
   *    break_statement `;`
   *    continue_statement `;`
   *    `discard` `;`
   *    variable_updating_statement `;`
   *    const_assert_statement `;`
   */
  Result<ast::Statement*> NonBlockStatement();

  /**
   * parse if statement, capturing the condition and body statement.
   */
  Result<IfInfo> ParseIf();

  /**
   * if_statement:
   *    attribute* if_clause else_if_clause* else_clause?
   *
   * if_clause:
   *    `if` expression compound_stmt
   *
   * else_if_clause:
   *    `else if` expression compound_stmt
   *
   * else_clause:
   *    `else` compound_statement
   */
  Result<ast::IfStatement*> IfStatement(AttrList& attrs);

  /**
   * case_selector:
   *    DEFAULT
   *    expression
   */
  Result<ast::CaseSelector*> CaseSelector();

  /**
   * case_selectors:
   *    case_selector (COMMA case_selector)* COMMA?
   */
  Result<CaseSelectorList> CaseSelectors();

  /**
   * switch_body:
   *    `case` case_selectors `:`? compound_statement
   *    `default` `:`? compound_statement
   */
  Result<ast::CaseStatement*> SwitchBody();

  /**
   * switch_statement:
   *    attribute* `switch` expression `{` switch_body+ `}`
   */
  Result<ast::SwitchStatement*> SwitchStatement(AttrList& attrs);

  /**
   * break_if_statement:
   *    `break` `if` expression `;`
   */
  Result<ast::Statement*> BreakIfStatement();

  /**
   * continuing_compound_statement:
   *    attribute* `{` statement* break_if_statement? `}`
   */
  Result<ast::BlockStatement*> ContinuingCompoundStatement();

  /**
   * continuing_statement:
   *    `continue` continuing_compound_statement
   */
  Result<ast::BlockStatement*> ContinuingStatement();

  /**
   * loop_statement:
   *    attribute* `loop` attribute* `{` statements continuing_statement? `}`
   */
  Result<ast::LoopStatement*> LoopStatement(AttrList& attrs);

  /**
   * (variable_updating | function_call)?
   */
  Result<ast::Statement*> ForHeaderContinuing();

  /**
   * for_header_initializer:
   *    variable_statement
   *    variable_updating_statement
   *    function_call_statement
   */
  Result<ast::Statement*> ForHeaderInitializer();

  /**
   * for_header:
   *    for_header_initializer? `;` expression? `;` for_header_continuing?
   */
  Result<ForHeader> ParseForHeader();

  /**
   * for_statement:
   *    `for` `(` for_header `)` compound_statement
   */
  Result<ast::ForLoopStatement*> ForStatement(AttrList& attrs);

  /**
   * while_statement:
   *    attribute* `while` expression compound_statement
   */
  Result<ast::WhileLoopStatement*> WhileStatement(AttrList& attrs);

  /**
   * statement:
   *    `;`
   *    if_statement
   *    switch_statement
   *    loop_statement
   *    for_statement
   *    while_statement
   *    compound_statement
   *    no_block_statement
   */
  Result<ast::Statement*> Statement();

  /**
   * statements:
   *    statement*
   */
  Result<StatementList> Statements();

  Result<ast::BlockStatement*> CompoundStatement(AttrList& attrs);
  /**
   * compound_statement:
   *    attribute* `{` statement* `}`
   */
  Result<ast::BlockStatement*> CompoundStatement();

  /**
   * function_decl:
   *    function_header compound_statement
   */
  Result<ast::Function*> FunctionDeclaration(
      std::vector<ast::Attribute*>& attrs);

  /**
   * variable_qualifier:
   *    `<` expression (`,` expression)? `>`
   */
  Result<VarQualifier> VariableQualifier();

  /**
   *  identifier
   */
  Result<ast::Identifier*> Identifier();

  /**
   * type_specifier:
   *    identifier template_arguments?
   */
  Result<ast::Type> TypeSpecifier();

  /**
   *  optionally_typed_ident:
   *    ident ( `:` typed_decl )?
   */
  Result<TypeIdentifier> IdentWithOptionalTypeSpec(bool allow_inferred);

  /**
   * variable_decl:
   *    `var` variable_qualifier? optionally_typed_ident
   */
  Result<VarDeclInfo> VariableDeclaration();

  Result<AttrList> AttributeList();

  /**
   * attribute:
   * ATTR identifier PAREN_LEFT expression ( COMMA expression )? COMMA?
   * PAREN_RIGHT
   *
   * ATTR `align` PAREN_LEFT expression COMMA? PAREN_RIGHT
   *
   * ATTR `binding` PAREN_LEFT expression COMMA? PAREN_RIGHT
   *
   * ATTR `builtin` PAREN_LEFT builtin_value_name COMMA? PAREN_RIGHT
   *
   * ATTR `const`
   *
   * ATTR 'diagnostic' diagnostic_control  ---   do not parse this
   *
   * ATTR `group` PAREN_LEFT expression COMMA? PAREN_RIGHT
   *
   * ATTR `id` PAREN_LEFT expression COMMA? PAREN_RIGHT
   *
   * ATTR `interpolate` PAREN_LEFT interpolate_type_name COMMA? PAREN_RIGHT
   *
   * ATTR `interpolate` PAREN_LEFT interpolate_type_name COMMA
   * interpolate_sampling_name COMMA? PAREN_RIGHT
   *
   * ATTR `invariant`
   *
   * ATTR `location` PAREN_LEFT expression COMMA? PAREN_RIGHT
   *
   * ATTR `must_use`
   *
   * ATTR `size` PAREN_LEFT expression COMMA? PAREN_RIGHT
   *
   * ATTR `workgroup_size` PAREN_LEFT expression [COMMA [ expression COMMA [
   * expression COMMA ]? ]? ]? PAREN_RIGHT
   *
   * ATTR `vertex`
   * ATTR `fragment`
   * ATTR `compute`
   *
   * @note: we do not parse diagnostic attribute
   */
  Result<ast::Attribute*> Attribute();

  /**
   * const_literal:
   *    INT_LITERAL
   *   | FLOAT_LITERAL
   *   | BOOL_LITERAL
   */
  Result<ast::Expression*> ConstLiteral();

  /**
   * primary_expression:
   *  const_literal
   *  | IDENT argument_expression_list?
   *  | paren_expression
   */
  Result<ast::Expression*> PrimaryExpression();

  /**
   * component_or_swizzle_specifier:
   *    : [empty]
   *     `[` expression `]` component_or_swizzle_specifier?
   *     `.` member_ident component_or_swizzle_specifier?
   *     `.` swizzle_name component_or_swizzle_specifier?
   */
  Result<ast::Expression*> ComponentOrSwizzle(ast::Expression* prefix);

  /**
   * singular_expression:
   *    : primary_expression postfix_expr
   */
  Result<ast::Expression*> SingularExpression();

  /**
   *  unary_expression:
   *    singular_expression
   *    `-` unary_expression
   *    `!` unary_expression
   *    `~` unary_expression
   *    `*` unary_expression
   *    `&` unary_expression
   */
  Result<ast::Expression*> UnaryExpression();

  /**
   * bitwise_expression.post.unary_expression:
   *    `&` unary_expression ( `&` unary_expression )*
   *    `|` unary_expression ( `|` unary_expression )*
   *    `^` unary_expression ( `^` unary_expression )*
   */
  Result<ast::Expression*> BitwiseExpPostUnaryExpr(ast::Expression* lhs);

  /**
   * additive_operator:
   *   MINUS
   *   PLUS
   */
  Result<ast::BinaryOp> AdditiveOp();

  /**
   * multiplicative_operator:
   *    FORWARD_SLASH
   *    MODULO
   *    STAR
   */
  Result<ast::BinaryOp> MultiplicativeOp();

  /**
   * additive_expression.pos.unary_expression:
   *    (additive_operator unary_expression
   * multiplicative_expression.post.unary_expression)*
   */
  Result<ast::Expression*> AdditiveExpPostUnaryExpr(ast::Expression* lhs);

  /**
   * multiplicative_expression.post.unary_expression:
   *   (multiplicative_operator unary_expression)*
   */
  Result<ast::Expression*> MultiplyExpPostUnaryExpr(ast::Expression* lhs);

  /**
   * math_expression.post.unary_expression:
   *    multiplicative_expression.post.unary_expression
   * additive_expression.post.unary_expression
   */
  Result<ast::Expression*> MathExpPostUnaryExpr(ast::Expression* lhs);

  /**
   * shift_expression:
   *     unary_expression shift_expression.post.unary_expression
   */
  Result<ast::Expression*> ShiftExpression();

  /**
   * relational_expression:
   *   unary_expression relational_expression.post.unary_expression
   */
  Result<ast::Expression*> RelationalExpression();

  /**
   * shift_expression.post.unary_expression:
   *    math_expression.post.unary_expression?
   *    `>>` unary_expression
   *    `<<` unary_expression
   */
  Result<ast::Expression*> ShiftExpPostUnaryExpr(ast::Expression* lhs);

  /**
   *  relational_expression.post.unary_expression:
   *    shift_expression.post.unary_expression
   *    shift_expression.post.unary_expression `==` shift_expression
   *    shift_expression.post.unary_expression `>` shift_expression
   *    shift_expression.post.unary_expression `>=` shift_expression
   *    shift_expression.post.unary_expression `<` shift_expression
   *    shift_expression.post.unary_expression `<=` shift_expression
   *    shift_expression.post.unary_expression `!=` shift_expression
   */
  Result<ast::Expression*> RelationExpPostUnaryExpr(ast::Expression* lhs);

  /**
   * expression:
   *    unary_expression bitwise_expression.post.unary_expression
   *
   *  | unary_expression relational_expression.post.unary_expression
   *
   *  |  unary_expression relational_expression.post.unary_expression `&&`
   *     relational_expression ( `&&` relational_expression )*
   *
   *  |   unary_expression relational_expression.post.unary_expression `||`
   *     relational_expression ( or_or relational_expression )*
   */
  Result<ast::Expression*> Expression();

  /**
   * expression [, expression]*
   */
  Result<std::vector<ast::Expression*>> ExpressionList();

 private:
  ast::NodeAllocator* allocator_;
  const std::vector<Token>& tokens_;
  Diagnosis diagnosis_ = {};
  ast::Module* module_ = nullptr;
  bool has_error_ = false;
  size_t token_index_ = 0;
};

}  // namespace wgx
