// Copyright 2020 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl/ast/type.h"

#include "wgsl/ast/expression.h"

namespace wgx {
namespace ast {

bool Type::IsBuiltin() const {
  if (expr == nullptr) {
    return false;
  }

  switch (expr->GetType()) {
    case ExpressionType::kIntLiteral:
    case ExpressionType::kFloatLiteral:
    case ExpressionType::kBoolLiteral:
      return true;
    case ExpressionType::kIdentifier: {
      // need to check if is vector or matrix
      auto* ident = static_cast<IdentifierExp*>(expr);
      if (ident->ident->name == "vec2" || ident->ident->name == "vec3" ||
          ident->ident->name == "vec4") {
        return true;
      }

      if (ident->ident->name == "mat2x2" || ident->ident->name == "mat2x3" ||
          ident->ident->name == "mat2x4" || ident->ident->name == "mat3x2" ||
          ident->ident->name == "mat3x3" || ident->ident->name == "mat3x4" ||
          ident->ident->name == "mat4x2" || ident->ident->name == "mat4x3" ||
          ident->ident->name == "mat4x4") {
        return true;
      }

      if (ident->ident->name == "bool" || ident->ident->name == "f32" ||
          ident->ident->name == "i32" || ident->ident->name == "u32") {
        return true;
      }

      return false;
    }
    default:
      return false;
  }
}

bool Type::IsArray() const {
  if (expr == nullptr) {
    return false;
  }

  if (expr->ident->name != "array") {
    return false;
  }

  const auto& args = expr->ident->args;

  if (args.size() != 2) {
    return false;
  }

  if (args[0]->GetType() != ExpressionType::kIdentifier ||
      args[1]->GetType() != ExpressionType::kIntLiteral) {
    return false;
  }

  return true;
}

Array Type::AsArray() const {
  if (!IsArray()) {
    return {};
  }

  const auto& args = expr->ident->args;

  return {static_cast<IdentifierExp*>(args[0]),
          static_cast<IntLiteralExp*>(args[1])};
}

}  // namespace ast
}  // namespace wgx
