// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <wgsl_cross.h>

#include "wgsl/ast/node.h"
#include "wgsl/function.h"
#include "wgsl/parser.h"
#include "wgsl/scanner.h"

#ifdef WGX_GLSL
#include "glsl/ast_printer.h"
#endif

#ifdef WGX_MSL
#include "msl/ast_printer.h"
#endif

namespace wgx {

Result Program::WriteToGlsl(const char *entry_point, const GlslOptions &options,
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

  glsl::AstPrinter printer{options, entry_point_func.get(), ctx};

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

Result Program::WriteToMsl(const char *entry_point, const MslOptions &options,
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

  msl::AstPrinter printer{options, entry_point_func.get(), ctx};

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

std::vector<BindGroup> Program::GetWGSLBindGroups(
    const char *entry_point) const {
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

bool Program::BuildAST(const std::vector<Token> &toke_list) {
  Parser parser{ast_allocator_, toke_list};

  module_ = parser.BuildModule();

  if (module_ == nullptr) {
    mDiagnosis = parser.GetDiagnosis();
  }

  return module_ != nullptr;
}

}  // namespace wgx