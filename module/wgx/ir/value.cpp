/*
 * Copyright 2021 The Lynx Authors. All rights reserved.
 * Licensed under the Apache License Version 2.0 that can be found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "ir/value.h"

namespace wgx {
namespace ir {

/** ========================================================================
 *  Value factory implementations
 *  ======================================================================== */

Value Value::None() {
  return Value();
}

Value Value::ConstantF32(TypeId type, float value) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kF32;
  v.const_data.f32 = value;
  return v;
}

Value Value::ConstantI32(TypeId type, int32_t value) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kI32;
  v.const_data.i32 = value;
  return v;
}

Value Value::ConstantU32(TypeId type, uint32_t value) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kU32;
  v.const_data.u32 = value;
  return v;
}

Value Value::ConstantBool(TypeId type, bool value) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kBool;
  v.const_data.boolean = value;
  return v;
}

Value Value::ConstantVec2F32(TypeId type, float x, float y) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kVec2F32;
  v.const_data.vec4[0] = x;
  v.const_data.vec4[1] = y;
  v.const_data.vec4[2] = 0.0f;
  v.const_data.vec4[3] = 0.0f;
  return v;
}

Value Value::ConstantVec3F32(TypeId type, float x, float y, float z) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kVec3F32;
  v.const_data.vec4[0] = x;
  v.const_data.vec4[1] = y;
  v.const_data.vec4[2] = z;
  v.const_data.vec4[3] = 0.0f;
  return v;
}

Value Value::ConstantVec4F32(TypeId type, float x, float y, float z, float w) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kVec4F32;
  v.const_data.vec4[0] = x;
  v.const_data.vec4[1] = y;
  v.const_data.vec4[2] = z;
  v.const_data.vec4[3] = w;
  return v;
}

Value Value::ConstantVec4F32(TypeId type, const std::array<float, 4>& values) {
  return ConstantVec4F32(type, values[0], values[1], values[2], values[3]);
}

Value Value::ConstantPoolRef(TypeId type, uint32_t pool_index) {
  Value v;
  v.kind = ValueKind::kConstant;
  v.type = type;
  v.const_kind = InlineConstKind::kNone;  /** Indicates pool reference */
  v.const_data.pool_index = pool_index;
  return v;
}

Value Value::SSA(TypeId type, uint32_t ssa_id) {
  Value v;
  v.kind = ValueKind::kSSA;
  v.type = type;
  v.id = ssa_id;
  return v;
}

Value Value::Variable(TypeId var_type, uint32_t var_id) {
  Value v;
  v.kind = ValueKind::kVariable;
  v.type = var_type;
  v.id = var_id;
  return v;
}

/** ========================================================================
 *  Safe value extraction implementations
 *  ======================================================================== */

std::optional<uint32_t> Value::GetSSAId() const {
  if (kind != ValueKind::kSSA) {
    return std::nullopt;
  }
  return id;
}

std::optional<uint32_t> Value::GetVarId() const {
  if (kind != ValueKind::kVariable) {
    return std::nullopt;
  }
  return id;
}

std::array<float, 4> Value::GetVec4F32() const {
  if (kind != ValueKind::kConstant || const_kind != InlineConstKind::kVec4F32) {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }
  return const_data.vec4;
}

float Value::GetF32() const {
  if (kind != ValueKind::kConstant || const_kind != InlineConstKind::kF32) {
    return 0.0f;
  }
  return const_data.f32;
}

int32_t Value::GetI32() const {
  if (kind != ValueKind::kConstant || const_kind != InlineConstKind::kI32) {
    return 0;
  }
  return const_data.i32;
}

uint32_t Value::GetU32() const {
  if (kind != ValueKind::kConstant || const_kind != InlineConstKind::kU32) {
    return 0;
  }
  return const_data.u32;
}

bool Value::GetBool() const {
  if (kind != ValueKind::kConstant || const_kind != InlineConstKind::kBool) {
    return false;
  }
  return const_data.boolean;
}

std::optional<uint32_t> Value::GetPoolIndex() const {
  if (kind != ValueKind::kConstant || const_kind != InlineConstKind::kNone) {
    return std::nullopt;
  }
  return const_data.pool_index;
}

/** ========================================================================
 *  ConstantPool implementation (placeholder)
 *  ======================================================================== */

class ConstantPool::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
  /** TODO: Add actual storage for complex constants */
};

ConstantPool::ConstantPool() : impl_(std::make_unique<Impl>()) {}

ConstantPool::~ConstantPool() = default;

}  // namespace ir
}  // namespace wgx
