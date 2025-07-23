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
#include <string_view>

#include "wgsl/ast/node.h"

namespace wgx {
namespace ast {

struct Expression;

enum class AttributeType {
  kUndefined,
  kAlign,
  kBinding,
  kBlendSrc,
  kBuiltin,
  kConst,
  kColor,
  kDiagnostic,
  kGroup,
  kId,
  kInterpolate,
  kInvariant,
  kLocation,
  kMustUse,
  kSize,
  kWorkgroupSize,
  kVertex,
  kFragment,
  kCompute,
};

/**
 * Base class for all attributes node
 *  All attribute is begin with a '@' in source code
 */
struct Attribute : public Node {
  ~Attribute() override = default;

  /**
   * The name of this attribute
   */
  virtual std::string GetName() const = 0;

  virtual AttributeType GetType() const = 0;

  void Accept(AstVisitor* visitor) override;
};

/**
 * Used for attribute which only contains name
 * For example:
 *   `@vertex`
 *   `@fragment`
 */
struct NamedAttribute : public Attribute {
  NamedAttribute(std::string name, AttributeType type);

  ~NamedAttribute() override = default;

  std::string GetName() const override;

  AttributeType GetType() const override;

 private:
  std::string name_;
  AttributeType type_;
};

struct AlignAttribute : public Attribute {
  explicit AlignAttribute(int64_t offset);
  ~AlignAttribute() override = default;

  std::string GetName() const override;

  AttributeType GetType() const override;

  int64_t offset;
};

struct BindingAttribute : public Attribute {
  explicit BindingAttribute(int64_t index);
  ~BindingAttribute() override = default;

  std::string GetName() const override;

  AttributeType GetType() const override;

  int64_t index;
};

struct BuiltinAttribute : public Attribute {
  explicit BuiltinAttribute(std::string_view name);
  ~BuiltinAttribute() override = default;

  std::string GetName() const override;

  AttributeType GetType() const override;

  std::string_view name;
};

struct GroupAttribute : public Attribute {
  explicit GroupAttribute(int64_t index);
  ~GroupAttribute() override = default;

  std::string GetName() const override;

  AttributeType GetType() const override;

  int64_t index;
};

struct LocationAttribute : public Attribute {
  explicit LocationAttribute(int64_t index);
  ~LocationAttribute() override = default;

  std::string GetName() const override;

  AttributeType GetType() const override;

  int64_t index;
};

enum class InterpolateType {
  kUndefined,
  kFlat,
  kLinear,
  kPerspective,
};

enum class InterpolateSampling {
  kUndefined,
  kCenter,
  kCentroid,
  kSample,
  kFirst,
  kEither,
};

struct InterpolateAttribute : public Attribute {
  InterpolateAttribute(InterpolateType type, InterpolateSampling sampling);

  ~InterpolateAttribute() override = default;

  std::string GetName() const override;

  AttributeType GetType() const override;

  static InterpolateType ParseType(std::string_view type_str);

  static InterpolateSampling ParseSampling(std::string_view sampling_str);

  InterpolateType type;
  InterpolateSampling sampling;
};

}  // namespace ast
}  // namespace wgx
