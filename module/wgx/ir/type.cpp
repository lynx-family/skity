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

  Type sampler_type;
  sampler_type.kind = TypeKind::kSampler;
  sampler_type_id_ = RegisterType(sampler_type);
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

TypeId TypeTable::GetTexture2DType(TypeId sampled_type) {
  if (sampled_type == kInvalidTypeId || !IsScalarType(sampled_type)) {
    return kInvalidTypeId;
  }

  Type type;
  type.kind = TypeKind::kTexture2D;
  type.element_type = sampled_type;

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

bool TypeTable::IsArrayType(TypeId id) const {
  const Type* type = GetType(id);
  return type != nullptr && type->kind == TypeKind::kArray;
}

bool TypeTable::IsSamplerType(TypeId id) const {
  const Type* type = GetType(id);
  return type != nullptr && type->kind == TypeKind::kSampler;
}

bool TypeTable::IsTextureType(TypeId id) const {
  const Type* type = GetType(id);
  return type != nullptr && type->kind == TypeKind::kTexture2D;
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

TypeId TypeTable::GetIndexedElementType(TypeId id) {
  const Type* type = GetType(id);
  if (type == nullptr) {
    return kInvalidTypeId;
  }

  if (type->kind == TypeKind::kVector || type->kind == TypeKind::kArray) {
    return type->element_type;
  }

  if (type->kind == TypeKind::kMatrix) {
    return GetVectorType(type->element_type, type->count);
  }

  return kInvalidTypeId;
}

uint32_t TypeTable::AlignOffset(uint32_t offset, uint32_t alignment) {
  return (offset + alignment - 1) & ~(alignment - 1);
}

TypeTable::LayoutInfo TypeTable::GetLayoutInfo(TypeId id,
                                               LayoutRule rule) const {
  const Type* type = GetType(id);
  if (type == nullptr) {
    return LayoutInfo{0, 1};
  }

  switch (type->kind) {
    case TypeKind::kBool:
    case TypeKind::kI32:
    case TypeKind::kU32:
    case TypeKind::kF32:
      // 4-byte scalar
      return LayoutInfo{4, 4};

    case TypeKind::kVector: {
      uint32_t component_size = 4;  // All supported components are 4 bytes
      uint32_t num_components = type->count;

      if (rule == LayoutRule::kStd140) {
        // std140: vec2 = 8 bytes, align 8; vec3/vec4 = 16 bytes, align 16
        if (num_components == 2) {
          return LayoutInfo{8, 8};
        } else {
          // vec3 and vec4 both use 16 bytes in std140
          return LayoutInfo{16, 16};
        }
      } else {
        // std430: vec2 = 8 bytes, align 8; vec3 = 12 bytes, align 16; vec4 = 16
        // bytes, align 16
        if (num_components == 2) {
          return LayoutInfo{8, 8};
        } else if (num_components == 3) {
          return LayoutInfo{12, 16};
        } else {
          return LayoutInfo{16, 16};
        }
      }
    }

    case TypeKind::kMatrix: {
      // Matrix is treated as array of column vectors
      uint32_t rows = type->count;
      uint32_t cols = type->count2;
      uint32_t vec_align = (rule == LayoutRule::kStd140 || cols >= 3) ? 16 : 8;
      uint32_t vec_size = cols * 4;
      if (rule == LayoutRule::kStd140 && cols == 3) {
        vec_size = 16;  // vec3 takes 16 bytes in std140
      }

      // Array stride is rounded up to alignment
      uint32_t stride = AlignOffset(vec_size, vec_align);
      uint32_t total_size = stride * rows;
      return LayoutInfo{total_size, vec_align};
    }

    case TypeKind::kArray: {
      auto elem_info = GetLayoutInfo(type->element_type, rule);
      uint32_t elem_align = elem_info.alignment;
      uint32_t elem_size = elem_info.size;

      // Array stride is rounded up to alignment
      uint32_t stride = AlignOffset(elem_size, elem_align);
      if (rule == LayoutRule::kStd140) {
        // std140: array stride must be multiple of 16
        stride = AlignOffset(stride, 16);
      }

      uint32_t total_size = stride * type->count;
      return LayoutInfo{total_size, elem_align};
    }

    case TypeKind::kStruct: {
      // Struct alignment is max of member alignments
      // Struct size is aligned to struct alignment
      uint32_t max_align = 1;
      uint32_t offset = 0;

      for (const auto& member : type->members) {
        auto member_info = GetLayoutInfo(member.type, rule);
        max_align = std::max(max_align, member_info.alignment);
        // Member offset should already be set, but calculate if not
        uint32_t member_offset =
            (member.offset == 0 && &member != &type->members.front())
                ? AlignOffset(offset, member_info.alignment)
                : member.offset;
        offset = member_offset + member_info.size;
      }

      // Round up struct size to its alignment
      uint32_t struct_size = AlignOffset(offset, max_align);
      return LayoutInfo{struct_size, max_align};
    }

    default:
      return LayoutInfo{0, 1};
  }
}

}  // namespace ir
}  // namespace wgx
