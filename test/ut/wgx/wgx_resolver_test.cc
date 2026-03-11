// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "module/wgx/semantic/resolver.h"
#include "module/wgx/wgsl/ast/expression.h"
#include "module/wgx/wgsl/ast/function.h"
#include "module/wgx/wgsl/ast/module.h"
#include "module/wgx/wgsl/ast/node.h"
#include "module/wgx/wgsl/ast/statement.h"
#include "module/wgx/wgsl/ast/variable.h"
#include "module/wgx/wgsl/parser.h"
#include "module/wgx/wgsl/scanner.h"

namespace {

class ResolverTest : public ::testing::Test {
 protected:
  wgx::semantic::BindResult Resolve(const std::string& source) {
    wgx::Scanner scanner{source};
    auto tokens = scanner.Scan();

    wgx::ast::NodeAllocator allocator;
    wgx::Parser parser{&allocator, tokens};
    wgx::ast::Module* module = parser.BuildModule();

    if (module == nullptr) {
      ADD_FAILURE() << parser.GetDiagnosis().message;
      return {};
    }

    wgx::semantic::Resolver resolver{module};
    return resolver.Resolve();
  }
};

TEST_F(ResolverTest, ResolvesShadowedVariablesWithoutDiagnostic) {
  const std::string source = R"(
fn helper(v: f32) -> f32 {
  var x: f32 = v;
  {
    var x: f32 = x + 1.0;
    x = x + 2.0;
  }
  return x;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let x: f32 = helper(1.0);
  return vec4<f32>(x, x, 0.0, 1.0);
}
)";

  wgx::Scanner scanner{source};
  auto tokens = scanner.Scan();

  wgx::ast::NodeAllocator allocator;
  wgx::Parser parser{&allocator, tokens};
  wgx::ast::Module* module = parser.BuildModule();

  ASSERT_NE(module, nullptr) << parser.GetDiagnosis().message;

  wgx::semantic::Resolver resolver{module};
  auto result = resolver.Resolve();

  ASSERT_TRUE(result.diagnostics.empty());
  ASSERT_FALSE(result.ident_symbols.empty());

  auto* helper = module->GetFunction("helper");
  ASSERT_NE(helper, nullptr);
  ASSERT_NE(helper->body, nullptr);
  ASSERT_GE(helper->body->statements.size(), 2u);

  ASSERT_EQ(helper->body->statements[0]->GetType(),
            wgx::ast::StatementType::kVarDecl);
  auto* outer_decl_stmt =
      static_cast<wgx::ast::VarDeclStatement*>(helper->body->statements[0]);
  ASSERT_NE(outer_decl_stmt, nullptr);
  ASSERT_NE(outer_decl_stmt->variable, nullptr);
  auto* outer_x_decl = outer_decl_stmt->variable;

  ASSERT_EQ(helper->body->statements[1]->GetType(),
            wgx::ast::StatementType::kBlock);
  auto* inner_block =
      static_cast<wgx::ast::BlockStatement*>(helper->body->statements[1]);
  ASSERT_NE(inner_block, nullptr);
  ASSERT_FALSE(inner_block->statements.empty());

  ASSERT_EQ(inner_block->statements[0]->GetType(),
            wgx::ast::StatementType::kVarDecl);
  auto* inner_decl_stmt =
      static_cast<wgx::ast::VarDeclStatement*>(inner_block->statements[0]);
  ASSERT_NE(inner_decl_stmt, nullptr);
  ASSERT_NE(inner_decl_stmt->variable, nullptr);
  auto* inner_x_decl = inner_decl_stmt->variable;
  ASSERT_NE(inner_x_decl->initializer, nullptr);
  ASSERT_EQ(inner_x_decl->initializer->GetType(),
            wgx::ast::ExpressionType::kBinaryExp);

  auto* init_binary =
      static_cast<wgx::ast::BinaryExp*>(inner_x_decl->initializer);
  ASSERT_NE(init_binary->lhs, nullptr);
  ASSERT_EQ(init_binary->lhs->GetType(), wgx::ast::ExpressionType::kIdentifier);
  auto* init_x_ident = static_cast<wgx::ast::IdentifierExp*>(init_binary->lhs);

  auto FindSymbolByDeclaration = [&](const wgx::ast::Node* declaration) {
    for (const auto& symbol : result.symbols) {
      if (symbol != nullptr && symbol->declaration == declaration) {
        return symbol.get();
      }
    }
    return static_cast<wgx::semantic::Symbol*>(nullptr);
  };

  auto* outer_x_symbol = FindSymbolByDeclaration(outer_x_decl);
  auto* inner_x_symbol = FindSymbolByDeclaration(inner_x_decl);
  ASSERT_NE(outer_x_symbol, nullptr);
  ASSERT_NE(inner_x_symbol, nullptr);
  ASSERT_NE(outer_x_symbol, inner_x_symbol);

  auto init_x_binding = result.ident_symbols.find(init_x_ident);
  ASSERT_NE(init_x_binding, result.ident_symbols.end());
  EXPECT_EQ(init_x_binding->second, outer_x_symbol);
  EXPECT_NE(init_x_binding->second, inner_x_symbol);
}

TEST_F(ResolverTest, ReportsDuplicateLocalDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var value: f32 = 1.0;
  let value: f32 = 2.0;
  return vec4<f32>(value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("duplicate"),
            std::string::npos);
}

TEST_F(ResolverTest, ReportsUnresolvedIdentifier) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(missing_value, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("unresolved identifier"),
            std::string::npos);
}

TEST_F(ResolverTest, AcceptsBuiltinFunctionCall) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let v: f32 = max(1.0, 2.0);
  return vec4<f32>(v, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, ResolvesBuiltinSymbolReferences) {
  const std::string source = R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let v: vec4<f32> = vec4<f32>(abs(-1.0), 0.0, 0.0, 1.0);
  return v;
}
)";

  wgx::Scanner scanner{source};
  auto tokens = scanner.Scan();

  wgx::ast::NodeAllocator allocator;
  wgx::Parser parser{&allocator, tokens};
  wgx::ast::Module* module = parser.BuildModule();
  ASSERT_NE(module, nullptr) << parser.GetDiagnosis().message;

  wgx::semantic::Resolver resolver{module};
  auto result = resolver.Resolve();
  ASSERT_TRUE(result.diagnostics.empty());

  wgx::semantic::Symbol* builtin_abs = nullptr;
  wgx::semantic::Symbol* builtin_vec4 = nullptr;
  for (const auto& symbol : result.symbols) {
    if (symbol == nullptr) {
      continue;
    }
    if (symbol->original_name == "abs" &&
        symbol->kind == wgx::semantic::SymbolKind::kBuiltinFunction) {
      builtin_abs = symbol.get();
    } else if (symbol->original_name == "vec4" &&
               symbol->kind == wgx::semantic::SymbolKind::kBuiltinType) {
      builtin_vec4 = symbol.get();
    }
  }
  ASSERT_NE(builtin_abs, nullptr);
  ASSERT_NE(builtin_vec4, nullptr);

  auto* vs_main = module->GetFunction("vs_main");
  ASSERT_NE(vs_main, nullptr);
  ASSERT_NE(vs_main->body, nullptr);
  ASSERT_FALSE(vs_main->body->statements.empty());

  ASSERT_EQ(vs_main->body->statements[0]->GetType(),
            wgx::ast::StatementType::kVarDecl);
  auto* let_stmt =
      static_cast<wgx::ast::VarDeclStatement*>(vs_main->body->statements[0]);
  ASSERT_NE(let_stmt, nullptr);
  ASSERT_NE(let_stmt->variable, nullptr);
  ASSERT_NE(let_stmt->variable->initializer, nullptr);

  ASSERT_EQ(let_stmt->variable->initializer->GetType(),
            wgx::ast::ExpressionType::kFuncCall);
  auto* vec_ctor =
      static_cast<wgx::ast::FunctionCallExp*>(let_stmt->variable->initializer);
  ASSERT_NE(vec_ctor->ident, nullptr);
  ASSERT_FALSE(vec_ctor->args.empty());

  ASSERT_EQ(vec_ctor->args[0]->GetType(), wgx::ast::ExpressionType::kFuncCall);
  auto* abs_call = static_cast<wgx::ast::FunctionCallExp*>(vec_ctor->args[0]);
  ASSERT_NE(abs_call->ident, nullptr);

  auto vec4_binding = result.ident_symbols.find(vec_ctor->ident);
  ASSERT_NE(vec4_binding, result.ident_symbols.end());
  EXPECT_EQ(vec4_binding->second, builtin_vec4);
  EXPECT_EQ(vec4_binding->second->kind,
            wgx::semantic::SymbolKind::kBuiltinType);

  auto abs_binding = result.ident_symbols.find(abs_call->ident);
  ASSERT_NE(abs_binding, result.ident_symbols.end());
  EXPECT_EQ(abs_binding->second, builtin_abs);
  EXPECT_EQ(abs_binding->second->kind,
            wgx::semantic::SymbolKind::kBuiltinFunction);
}

TEST_F(ResolverTest,
       RecordsDeclarationSymbolsForStructMembersParametersAndLocals) {
  const std::string source = R"(
struct Payload {
  value: f32,
}

fn helper(param: f32) -> f32 {
  let local: f32 = param;
  return local;
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var output: Payload;
  output.value = helper(1.0);
  return vec4<f32>(output.value, 0.0, 0.0, 1.0);
}
)";

  wgx::Scanner scanner{source};
  auto tokens = scanner.Scan();

  wgx::ast::NodeAllocator allocator;
  wgx::Parser parser{&allocator, tokens};
  wgx::ast::Module* module = parser.BuildModule();
  ASSERT_NE(module, nullptr) << parser.GetDiagnosis().message;

  wgx::semantic::Resolver resolver{module};
  auto result = resolver.Resolve();
  ASSERT_TRUE(result.diagnostics.empty());

  auto* payload =
      static_cast<wgx::ast::StructDecl*>(module->GetGlobalTypeDecl("Payload"));
  ASSERT_NE(payload, nullptr);
  ASSERT_FALSE(payload->members.empty());
  ASSERT_NE(payload->members[0]->name, nullptr);
  EXPECT_NE(result.decl_symbols.find(payload->members[0]->name),
            result.decl_symbols.end());

  auto* helper = module->GetFunction("helper");
  ASSERT_NE(helper, nullptr);
  ASSERT_EQ(helper->params.size(), 1u);
  ASSERT_NE(helper->params[0]->name, nullptr);
  EXPECT_NE(result.decl_symbols.find(helper->params[0]->name),
            result.decl_symbols.end());

  ASSERT_NE(helper->body, nullptr);
  ASSERT_FALSE(helper->body->statements.empty());
  auto* local_decl_stmt =
      static_cast<wgx::ast::VarDeclStatement*>(helper->body->statements[0]);
  ASSERT_NE(local_decl_stmt, nullptr);
  ASSERT_NE(local_decl_stmt->variable, nullptr);
  ASSERT_NE(local_decl_stmt->variable->name, nullptr);
  EXPECT_NE(result.decl_symbols.find(local_decl_stmt->variable->name),
            result.decl_symbols.end());
}

TEST_F(ResolverTest, AcceptsBuiltinIdentifierReference) {
  auto result = Resolve(R"(
@group(0) @binding(0) var tex: texture_2d<f32>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  let dims: vec2<u32> = textureDimensions(tex);
  return vec4<f32>(f32(dims.x), 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, ReportsInvalidVectorTemplateArgCount) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var bad: vec4<f32, f32>;
  return vec4<f32>(bad.x, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("invalid type declaration"),
            std::string::npos);
}

TEST_F(ResolverTest, ReportsInvalidArrayDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var bad: array<f32>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("array expects 2"),
            std::string::npos);
}

TEST_F(ResolverTest, ReportsUnresolvedArrayElementType) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var bad: array<UnknownType, 4>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("unresolved type"),
            std::string::npos);
}

TEST_F(ResolverTest, ReportsInvalidArrayElementTypeDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var bad: array<vec4<f32, f32>, 4>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("vector expects 1"),
            std::string::npos);
}

TEST_F(ResolverTest, AcceptsCompositeArrayElementTypeDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var ok: array<vec2<f32>, 4>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, AcceptsAtomicTypeDeclaration) {
  auto result = Resolve(R"(
@group(0) @binding(0) var<storage> data: array<atomic<u32>, 4>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, ReportsInvalidAtomicTypeDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var bad: atomic<f32>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("atomic element type"),
            std::string::npos);
}

TEST_F(ResolverTest, AcceptsPtrTypeDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var p: ptr<function, f32, read_write>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, AcceptsCompositePtrPointeeTypeDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var p: ptr<function, vec4<f32>>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, ReportsInvalidPtrTypeDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var p: ptr<unknown_space, f32>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("ptr address space"),
            std::string::npos);
}

TEST_F(ResolverTest, ReportsInvalidPtrPointeeTypeDeclaration) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var p: ptr<function, vec4<f32, f32>>;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("vector expects 1"),
            std::string::npos);
}

TEST_F(ResolverTest, ReportsBuiltinFunctionAsInvalidTypeName) {
  auto result = Resolve(R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  var bad: abs;
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().message.find("is not a type"),
            std::string::npos);
}

TEST_F(ResolverTest, ResolvesStructMemberTypeWithoutMemberNameTypeShadowing) {
  auto result = Resolve(R"(
alias MyType = f32;

struct S {
  MyType: MyType,
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, AcceptsVarAddressSpaceAndAccessQualifiers) {
  auto result = Resolve(R"(
@group(0) @binding(0) var<storage, read_write> data: array<f32, 4>;

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest,
       HandlesBuiltinTypeLikeStructMemberNameWithoutTypeConflict) {
  auto result = Resolve(R"(
alias Scalar = f32;

struct S {
  Scalar: Scalar,
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest,
       ResolvesLaterStructMemberTypeWhenEarlierMemberShadowsName) {
  auto result = Resolve(R"(
alias Scalar = f32;

struct S {
  Scalar: i32,
  x: Scalar,
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)");

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST_F(ResolverTest, RejectsReservedBuiltinTypeNameAsStructMemberName) {
  const std::string source = R"(
struct S {
  f32: f32,
}

@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)";

  wgx::Scanner scanner{source};
  auto tokens = scanner.Scan();

  wgx::ast::NodeAllocator allocator;
  wgx::Parser parser{&allocator, tokens};
  wgx::ast::Module* module = parser.BuildModule();

  EXPECT_EQ(module, nullptr);
  EXPECT_NE(parser.GetDiagnosis().message.find(
                "Struct member name cannot use reserved builtin type 'f32'"),
            std::string::npos);
}

}  // namespace
