// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "spirv/emitter_internal.h"

namespace wgx {
namespace spirv {

TypeEmitter::TypeEmitter(IdAllocator* ids, SectionBuffers* sections,
                         ir::TypeTable* type_table)
    : ids_(ids), sections_(sections), type_table_(type_table) {}

uint32_t TypeEmitter::EmitType(ir::TypeId type_id) {
  if (type_id == ir::kInvalidTypeId) return 0;

  auto it = emitted_types_.find(type_id);
  if (it != emitted_types_.end()) {
    return it->second;
  }

  const ir::Type* type = type_table_->GetType(type_id);
  if (type == nullptr) return 0;

  uint32_t spirv_id = ids_->Allocate();
  emitted_types_[type_id] = spirv_id;

  switch (type->kind) {
    case ir::TypeKind::kVoid:
      AppendInstruction(&sections_->types_consts_globals, SpvOpTypeVoid,
                        {spirv_id});
      break;
    case ir::TypeKind::kBool:
      AppendInstruction(&sections_->types_consts_globals, SpvOpTypeBool,
                        {spirv_id});
      break;
    case ir::TypeKind::kI32:
      AppendInstruction(&sections_->types_consts_globals, SpvOpTypeInt,
                        {spirv_id, 32u, 1u});
      break;
    case ir::TypeKind::kU32:
      AppendInstruction(&sections_->types_consts_globals, SpvOpTypeInt,
                        {spirv_id, 32u, 0u});
      break;
    case ir::TypeKind::kF32:
      AppendInstruction(&sections_->types_consts_globals, SpvOpTypeFloat,
                        {spirv_id, 32u});
      break;
    case ir::TypeKind::kVector:
      EmitVectorType(type, spirv_id);
      break;
    case ir::TypeKind::kMatrix:
      EmitMatrixType(type, spirv_id);
      break;
    case ir::TypeKind::kArray:
      EmitArrayType(type, spirv_id);
      break;
    case ir::TypeKind::kStruct:
      EmitStructType(type, spirv_id);
      break;
    case ir::TypeKind::kPointer:
      EmitPointerType(type, spirv_id);
      break;
  }

  return spirv_id;
}

uint32_t TypeEmitter::GetFunctionTypeVoid() {
  if (function_void_type_ != 0) return function_void_type_;

  function_void_type_ = GetFunctionType(type_table_->GetVoidType(), {});
  return function_void_type_;
}

uint32_t TypeEmitter::GetFunctionType(
    ir::TypeId return_type, const std::vector<ir::TypeId>& param_types) {
  FunctionSignature signature;
  signature.return_type = return_type;
  signature.param_types = param_types;

  auto it = function_types_.find(signature);
  if (it != function_types_.end()) {
    return it->second;
  }

  uint32_t spirv_return_type = EmitType(return_type);
  if (spirv_return_type == 0) {
    return 0;
  }

  std::vector<uint32_t> operands;
  const uint32_t function_type_id = ids_->Allocate();
  operands.push_back(function_type_id);
  operands.push_back(spirv_return_type);
  for (ir::TypeId param_type : param_types) {
    uint32_t spirv_param_type = EmitType(param_type);
    if (spirv_param_type == 0) {
      return 0;
    }
    operands.push_back(spirv_param_type);
  }

  AppendInstruction(&sections_->types_consts_globals, SpvOpTypeFunction,
                    operands);
  function_types_[signature] = function_type_id;
  return function_type_id;
}

uint32_t TypeEmitter::GetPointerType(ir::TypeId pointee,
                                     SpvStorageClass storage) {
  auto key = std::make_pair(pointee, storage);
  auto it = pointer_types_.find(key);
  if (it != pointer_types_.end()) {
    return it->second;
  }

  uint32_t pointee_id = EmitType(pointee);
  if (pointee_id == 0) return 0;

  uint32_t ptr_id = ids_->Allocate();
  AppendInstruction(&sections_->types_consts_globals, SpvOpTypePointer,
                    {ptr_id, static_cast<uint32_t>(storage), pointee_id});
  pointer_types_[key] = ptr_id;
  return ptr_id;
}

uint32_t TypeEmitter::EmitF32Constant(float value) {
  uint32_t bits = FloatToBits(value);
  auto it = f32_constants_.find(bits);
  if (it != f32_constants_.end()) {
    return it->second;
  }

  uint32_t f32_type = EmitType(type_table_->GetF32Type());
  if (f32_type == 0) return 0;

  uint32_t const_id = ids_->Allocate();
  AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                    {f32_type, const_id, bits});
  f32_constants_[bits] = const_id;
  return const_id;
}

uint32_t TypeEmitter::EmitI32Constant(int32_t value) {
  auto it = i32_constants_.find(value);
  if (it != i32_constants_.end()) {
    return it->second;
  }

  uint32_t i32_type = EmitType(type_table_->GetI32Type());
  if (i32_type == 0) return 0;

  uint32_t const_id = ids_->Allocate();
  AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                    {i32_type, const_id, static_cast<uint32_t>(value)});
  i32_constants_[value] = const_id;
  return const_id;
}

uint32_t TypeEmitter::EmitU32Constant(uint32_t value) {
  auto it = u32_constants_.find(value);
  if (it != u32_constants_.end()) {
    return it->second;
  }

  uint32_t u32_type = EmitType(type_table_->GetU32Type());
  if (u32_type == 0) return 0;

  uint32_t const_id = ids_->Allocate();
  AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                    {u32_type, const_id, value});
  u32_constants_[value] = const_id;
  return const_id;
}

uint32_t TypeEmitter::EmitBoolConstant(bool value) {
  auto it = bool_constants_.find(value);
  if (it != bool_constants_.end()) {
    return it->second;
  }

  uint32_t bool_type = EmitType(type_table_->GetBoolType());
  if (bool_type == 0) return 0;

  uint32_t const_id = ids_->Allocate();
  auto opcode = value ? SpvOpConstantTrue : SpvOpConstantFalse;
  AppendInstruction(&sections_->types_consts_globals, opcode,
                    {bool_type, const_id});
  bool_constants_[value] = const_id;
  return const_id;
}

void TypeEmitter::EmitVectorType(const ir::Type* type, uint32_t spirv_id) {
  uint32_t component_id = EmitType(type->element_type);
  if (component_id == 0) return;
  AppendInstruction(&sections_->types_consts_globals, SpvOpTypeVector,
                    {spirv_id, component_id, type->count});
}

void TypeEmitter::EmitMatrixType(const ir::Type* type, uint32_t spirv_id) {
  uint32_t component_id = EmitType(type->element_type);
  if (component_id == 0) return;
  AppendInstruction(&sections_->types_consts_globals, SpvOpTypeMatrix,
                    {spirv_id, component_id, type->count2});
}

void TypeEmitter::EmitArrayType(const ir::Type* type, uint32_t spirv_id) {
  uint32_t element_id = EmitType(type->element_type);
  if (element_id == 0) return;
  uint32_t size_const = ids_->Allocate();
  uint32_t u32_type = EmitType(type_table_->GetU32Type());
  AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                    {u32_type, size_const, type->count});
  AppendInstruction(&sections_->types_consts_globals, SpvOpTypeArray,
                    {spirv_id, element_id, size_const});
}

void TypeEmitter::EmitStructType(const ir::Type* type, uint32_t spirv_id) {
  std::vector<uint32_t> operands;
  operands.push_back(spirv_id);
  for (const auto& member : type->members) {
    uint32_t member_type_id = EmitType(member.type);
    if (member_type_id == 0) return;
    operands.push_back(member_type_id);
  }
  AppendInstruction(&sections_->types_consts_globals, SpvOpTypeStruct,
                    operands);
}

void TypeEmitter::EmitPointerType(const ir::Type* type, uint32_t spirv_id) {
  uint32_t pointee_id = EmitType(type->element_type);
  if (pointee_id == 0) return;
  AppendInstruction(
      &sections_->types_consts_globals, SpvOpTypePointer,
      {spirv_id, static_cast<uint32_t>(ToSpvStorageClass(type->storage_class)),
       pointee_id});
}

size_t TypeEmitter::PairHash::operator()(
    const std::pair<ir::TypeId, SpvStorageClass>& p) const {
  return std::hash<ir::TypeId>{}(p.first) ^
         (std::hash<int>{}(static_cast<int>(p.second)) << 1);
}

size_t TypeEmitter::FunctionSignatureHash::operator()(
    const FunctionSignature& sig) const {
  size_t hash = std::hash<ir::TypeId>{}(sig.return_type);
  for (ir::TypeId param_type : sig.param_types) {
    hash ^= std::hash<ir::TypeId>{}(param_type) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
  }
  return hash;
}

}  // namespace spirv
}  // namespace wgx
