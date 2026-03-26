// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ir/type.h"

#include <functional>

namespace wgx {
namespace ir {

// Hash function for Type
size_t TypeHash::operator()(const Type& type) const {
  size_t h = static_cast<size_t>(type.kind);
  h = h * 31 + type.element_type;
  h = h * 31 + type.count;
  h = h * 31 + type.count2;
  h = h * 31 + static_cast<size_t>(type.storage_class);
  for (const auto& member : type.members) {
    h = h * 31 + member.type;
    h = h * 31 + std::hash<std::string>{}(member.name);
  }
  return h;
}

TypeTable::TypeTable() {
  // Register primitive types
  Type void_type;
  void_type.kind = TypeKind::kVoid;
  void_type_id_ = RegisterType(void_type);

  Type bool_type;
  bool_type.kind = TypeKind::kBool;
  bool_type_id_ = RegisterType(bool_type);

  Type i32_type;
  i32_type.kind = TypeKind::kI32;
  i32_type_id_ = RegisterType(i32_type);

  Type u32_type;
  u32_type.kind = TypeKind::kU32;
  u32_type_id_ = RegisterType(u32_type);

  Type f32_type;
  f32_type.kind = TypeKind::kF32;
  f32_type_id_ = RegisterType(f32_type);
}

TypeTable::~TypeTable() = default;

TypeId TypeTable::RegisterType(Type type) {
  // ID 0 is reserved for kInvalidTypeId, so start from 1
  TypeId id = static_cast<TypeId>(types_.size()) + 1;
  types_.push_back(std::make_unique<Type>(type));
  type_map_[type] = id;
  return id;
}

TypeId TypeTable::GetVectorType(TypeId component_type, uint32_t count) {
  if (component_type == kInvalidTypeId || count < 2 || count > 4) {
    return kInvalidTypeId;
  }

  Type type;
  type.kind = TypeKind::kVector;
  type.element_type = component_type;
  type.count = count;

  auto it = type_map_.find(type);
  if (it != type_map_.end()) {
    return it->second;
  }

  return RegisterType(type);
}

TypeId TypeTable::GetMatrixType(TypeId component_type, uint32_t rows,
                                uint32_t cols) {
  if (component_type == kInvalidTypeId || rows < 2 || rows > 4 || cols < 2 ||
      cols > 4) {
    return kInvalidTypeId;
  }

  Type type;
  type.kind = TypeKind::kMatrix;
  type.element_type = component_type;
  type.count = rows;
  type.count2 = cols;

  auto it = type_map_.find(type);
  if (it != type_map_.end()) {
    return it->second;
  }

  return RegisterType(type);
}

TypeId TypeTable::GetArrayType(TypeId element_type, uint32_t count) {
  if (element_type == kInvalidTypeId || count == 0) {
    return kInvalidTypeId;
  }

  Type type;
  type.kind = TypeKind::kArray;
  type.element_type = element_type;
  type.count = count;

  auto it = type_map_.find(type);
  if (it != type_map_.end()) {
    return it->second;
  }

  return RegisterType(type);
}

TypeId TypeTable::GetStructType(const std::vector<StructMember>& members) {
  // Validate members
  for (const auto& member : members) {
    if (member.type == kInvalidTypeId) {
      return kInvalidTypeId;
    }
  }

  Type type;
  type.kind = TypeKind::kStruct;
  type.members = members;

  auto it = type_map_.find(type);
  if (it != type_map_.end()) {
    return it->second;
  }

  return RegisterType(type);
}

TypeId TypeTable::GetPointerType(TypeId pointee, StorageClass storage) {
  if (pointee == kInvalidTypeId) {
    return kInvalidTypeId;
  }

  Type type;
  type.kind = TypeKind::kPointer;
  type.element_type = pointee;
  type.storage_class = storage;

  auto it = type_map_.find(type);
  if (it != type_map_.end()) {
    return it->second;
  }

  return RegisterType(type);
}

const Type* TypeTable::GetType(TypeId id) const {
  if (id == kInvalidTypeId || id > types_.size()) {
    return nullptr;
  }
  // IDs are 1-based, so subtract 1 for vector index
  return types_[id - 1].get();
}

bool TypeTable::IsScalarType(TypeId id) const {
  const Type* type = GetType(id);
  if (type == nullptr) {
    return false;
  }
  return type->kind == TypeKind::kBool || type->kind == TypeKind::kI32 ||
         type->kind == TypeKind::kU32 || type->kind == TypeKind::kF32;
}

bool TypeTable::IsIntegerType(TypeId id) const {
  const Type* type = GetType(id);
  if (type == nullptr) {
    return false;
  }
  return type->kind == TypeKind::kI32 || type->kind == TypeKind::kU32;
}

bool TypeTable::IsFloatType(TypeId id) const {
  const Type* type = GetType(id);
  if (type == nullptr) {
    return false;
  }
  return type->kind == TypeKind::kF32;
}

bool TypeTable::IsVectorType(TypeId id) const {
  const Type* type = GetType(id);
  return type != nullptr && type->kind == TypeKind::kVector;
}

bool TypeTable::IsMatrixType(TypeId id) const {
  const Type* type = GetType(id);
  return type != nullptr && type->kind == TypeKind::kMatrix;
}

uint32_t TypeTable::GetVectorComponentCount(TypeId id) const {
  const Type* type = GetType(id);
  if (type == nullptr || type->kind != TypeKind::kVector) {
    return 0;
  }
  return type->count;
}

TypeId TypeTable::GetComponentType(TypeId id) const {
  const Type* type = GetType(id);
  if (type == nullptr) {
    return kInvalidTypeId;
  }
  if (type->kind == TypeKind::kVector || type->kind == TypeKind::kMatrix) {
    return type->element_type;
  }
  return kInvalidTypeId;
}

}  // namespace ir
}  // namespace wgx
