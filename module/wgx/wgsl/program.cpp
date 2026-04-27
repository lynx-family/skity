// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <wgsl_cross.h>

#include <cstdarg>
#include <cstdio>

#include "semantic/resolver.h"
#include "wgsl/ast/node.h"
#include "wgsl/function.h"
#include "wgsl/parser.h"
#include "wgsl/scanner.h"

#ifdef WGX_VULKAN
#include "lower/lower_to_ir.h"
#include "spirv/emitter.h"
#endif

#ifdef WGX_GLSL
#include "glsl/ast_printer.h"
#endif

#ifdef WGX_MSL
#include "msl/ast_printer.h"
#endif

namespace wgx {

namespace {

void LogWgxError(const char* fmt, ...) {
#ifndef NDEBUG
  std::fprintf(stderr, "[skity] [ERROR]");
  va_list args;
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  va_end(args);
  std::fprintf(stderr, "\n");
#else
  (void)fmt;
#endif
}

}  // namespace

Result Program::WriteToGlsl(const char* entry_point, const GlslOptions& options,
                            std::optional<CompilerContext> ctx) const {
#ifdef WGX_GLSL
  auto func = module_->GetFunction(entry_point);

  if (func == nullptr || !func->IsEntryPoint()) {
    return {};
  }

  auto entry_point_func =
      Function::Create(module_, func, MemoryLayout::kStd140);

  if (entry_point_func == nullptr) {
    return {};
  }

  glsl::AstPrinter printer{options, entry_point_func.get(), ctx, ident_symbols_,
                           decl_symbols_};

  if (printer.Write()) {
    return {
        printer.GetResult(),
        entry_point_func->GetBindGroups(),
        {
            printer.GetUboIndex(),
            printer.GetTextureIndex(),
            0,
        },
    };
  }
#endif

  return {};
}

Result Program::WriteToMsl(const char* entry_point, const MslOptions& options,
                           std::optional<CompilerContext> ctx) const {
#ifdef WGX_MSL
  auto func = module_->GetFunction(entry_point);

  if (func == nullptr || !func->IsEntryPoint()) {
    return {};
  }

  auto entry_point_func =
      Function::Create(module_, func, MemoryLayout::kStd430MSL);

  if (entry_point_func == nullptr) {
    return {};
  }

  msl::AstPrinter printer{options, entry_point_func.get(), ctx, ident_symbols_,
                          decl_symbols_};

  if (printer.Write()) {
    return {
        printer.GetResult(),
        entry_point_func->GetBindGroups(),
        {
            printer.GetBufferIndex(),
            printer.GetTextureIndex(),
            printer.GetSamplerIndex(),
        },
    };
  }
#endif

  return {};
}

Result Program::WriteToSpirv(const char* entry_point,
                             const SpirvOptions& options,
                             std::optional<CompilerContext> ctx) const {
  (void)options;
  (void)ctx;
#ifdef WGX_VULKAN
  auto func = module_->GetFunction(entry_point);

  if (func == nullptr || !func->IsEntryPoint()) {
    LogWgxError("WGX SPIR-V emission failed: invalid entry point '%s'",
                entry_point != nullptr ? entry_point : "<null>");
    return {};
  }

  auto ir_module =
      lower::LowerToIR(module_, func, ident_symbols_, decl_symbols_);
  if (ir_module == nullptr) {
    LogWgxError(
        "WGX SPIR-V emission failed: LowerToIR returned null for entry '%s'",
        entry_point != nullptr ? entry_point : "<null>");
    return {};
  }

  spirv::Emitter emitter;
  if (!emitter.Emit(*ir_module)) {
    LogWgxError(
        "WGX SPIR-V emission failed: backend emitter failed for entry '%s'",
        entry_point != nullptr ? entry_point : "<null>");
    return {};
  }

  return {
      emitter.GetResult(),
      {},
      {},
  };
#endif

  return {};
}

std::vector<BindGroup> Program::GetWGSLBindGroups(
    const char* entry_point) const {
  auto func = module_->GetFunction(entry_point);

  if (func == nullptr || !func->IsEntryPoint()) {
    return {};
  }

  auto entry_point_func = Function::Create(module_, func, MemoryLayout::kWGSL);

  if (entry_point_func == nullptr) {
    return {};
  }

  return entry_point_func->GetBindGroups();
}

Program::Program(std::string source)
    : ast_allocator_(new ast::NodeAllocator), mSource(std::move(source)) {}

Program::~Program() { delete ast_allocator_; }

std::unique_ptr<Program> Program::Parse(std::string source) {
  std::unique_ptr<Program> p{new Program(std::move(source))};

  p->Parse();
  return p;
}

bool Program::Parse() {
  Scanner scanner{mSource};
  auto tokens = scanner.Scan();

  return BuildAST(tokens);
}

bool Program::BuildAST(const std::vector<Token>& toke_list) {
  Parser parser{ast_allocator_, toke_list};

  module_ = parser.BuildModule();

  if (module_ == nullptr) {
    mDiagnosis = parser.GetDiagnosis();
    return false;
  }

  semantic::Resolver resolver{module_};
  auto bind_result = resolver.Resolve();

  if (bind_result.HasError()) {
    mDiagnosis = bind_result.diagnostics.front();
    module_ = nullptr;
    return false;
  }

  ident_symbols_ = std::move(bind_result.ident_symbols);
  decl_symbols_ = std::move(bind_result.decl_symbols);
  symbols_ = std::move(bind_result.symbols);

  return true;
}

}  // namespace wgx
