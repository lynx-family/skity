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

#include "wgsl/ast/attribute.h"

#include "wgsl/ast/visitor.h"

namespace wgx {
namespace ast {

void Attribute::Accept(AstVisitor* visitor) { visitor->Visit(this); }

NamedAttribute::NamedAttribute(std::string name, AttributeType type)
    : name_(std::move(name)), type_(type) {}

std::string NamedAttribute::GetName() const { return name_; }

AttributeType NamedAttribute::GetType() const { return type_; }

AlignAttribute::AlignAttribute(int64_t offset) : offset(offset) {}

std::string AlignAttribute::GetName() const { return "align"; }

AttributeType AlignAttribute::GetType() const { return AttributeType::kAlign; }

BindingAttribute::BindingAttribute(int64_t index) : index(index) {}

std::string BindingAttribute::GetName() const { return "binding"; }

AttributeType BindingAttribute::GetType() const {
  return AttributeType::kBinding;
}

BuiltinAttribute::BuiltinAttribute(std::string_view name) : name(name) {}

std::string BuiltinAttribute::GetName() const { return "builtin"; }

AttributeType BuiltinAttribute::GetType() const {
  return AttributeType::kBuiltin;
}

GroupAttribute::GroupAttribute(int64_t index) : index(index) {}

std::string GroupAttribute::GetName() const { return "group"; }

AttributeType GroupAttribute::GetType() const { return AttributeType::kGroup; }

LocationAttribute::LocationAttribute(int64_t index) : index(index) {}

std::string LocationAttribute::GetName() const { return "location"; }

AttributeType LocationAttribute::GetType() const {
  return AttributeType::kLocation;
}

InterpolateAttribute::InterpolateAttribute(InterpolateType type,
                                           InterpolateSampling sampling)
    : type(type), sampling(sampling) {}

std::string InterpolateAttribute::GetName() const { return "interpolate"; }

AttributeType InterpolateAttribute::GetType() const {
  return AttributeType::kInterpolate;
}

InterpolateType InterpolateAttribute::ParseType(std::string_view type_str) {
  if (type_str == "flat") {
    return InterpolateType::kFlat;
  } else if (type_str == "linear") {
    return InterpolateType::kLinear;
  } else if (type_str == "perspective") {
    return InterpolateType::kPerspective;
  } else {
    return InterpolateType::kUndefined;
  }
}

InterpolateSampling InterpolateAttribute::ParseSampling(
    std::string_view sampling_str) {
  if (sampling_str == "center") {
    return InterpolateSampling::kCenter;
  } else if (sampling_str == "centroid") {
    return InterpolateSampling::kCentroid;
  } else if (sampling_str == "sample") {
    return InterpolateSampling::kSample;
  } else if (sampling_str == "first") {
    return InterpolateSampling::kFirst;
  } else if (sampling_str == "either") {
    return InterpolateSampling::kEither;
  } else {
    return InterpolateSampling::kUndefined;
  }
}

}  // namespace ast
}  // namespace wgx