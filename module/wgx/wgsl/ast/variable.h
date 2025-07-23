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

#pragma once

#include <string>
#include <vector>

#include "wgsl/ast/node.h"
#include "wgsl/ast/type.h"

namespace wgx {
namespace ast {

struct Expression;
struct Attribute;
struct Identifier;

enum class AttributeType;

enum class VariableType {
  kVar,
  kConst,
  kLet,
  kParameter,
};

struct Variable : public Node {
  Variable(Identifier* name, Type type, Expression* initializer,
           std::vector<Attribute*> attributes)
      : name(name),
        type(type),
        initializer(initializer),
        attributes(std::move(attributes)) {}

  ~Variable() override = default;

  virtual VariableType GetType() = 0;
  virtual std::string Kind() = 0;

  void Accept(AstVisitor* visitor) override;

  Attribute* GetAttribute(AttributeType type) const;

  Identifier* name;
  Type type;
  Expression* initializer;
  std::vector<Attribute*> attributes;
};

/**
 * var declaration:
 *
 * var<uniform> name: Type;
 * var name: type = initializer;
 */
struct Var : public Variable {
  Var(Identifier* name, Type type, Expression* address_space,
      Expression* access, Expression* initializer,
      std::vector<Attribute*> attributes);

  ~Var() override = default;

  std::string Kind() override;

  VariableType GetType() override;

  Expression* address_space = nullptr;
  Expression* access = nullptr;
};

struct ConstVar : public Variable {
  ConstVar(Identifier* name, Type type, Expression* initializer);

  ConstVar(Identifier* name, Type type, Expression* initializer,
           std::vector<Attribute*> attributes);

  ~ConstVar() override = default;

  std::string Kind() override;

  VariableType GetType() override;
};

struct LetVar : public Variable {
  LetVar(Identifier* name, Type type, Expression* initializer);
  LetVar(Identifier* name, Type type, Expression* initializer,
         std::vector<Attribute*> attributes);

  std::string Kind() override;

  VariableType GetType() override;
};

struct Parameter : public Variable {
  Parameter(Identifier* name, Type type, std::vector<Attribute*> attributes);
  ~Parameter() override = default;

  std::string Kind() override;

  VariableType GetType() override;
};

}  // namespace ast
}  // namespace wgx
