// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace wgx {
namespace ir {

// Type ID is an integer (0 = void/invalid)
using TypeId = uint32_t;
constexpr TypeId kInvalidTypeId = 0;

// Type kinds
enum class TypeKind {
  kVoid,
  kBool,
  kI32,
  kU32,
  kF32,
  kVector,   // component_type + count
  kMatrix,   // component_type + rows + cols
  kArray,    // element_type + count
  kStruct,   // members[]
  kPointer,  // storage_class + pointee_type
  kSampler,
  kTexture2D,  // sampled element type in element_type
};

// Storage class for pointers
enum class StorageClass {
  kFunction,
  kPrivate,
  kUniform,
  kStorage,
  kOutput,
  kInput,
  kWorkgroup,
  kHandle,  // for samplers/textures
};

// Struct member description
struct StructMember {
  TypeId type = kInvalidTypeId;
  std::string name;
  // Layout offset for uniform/storage buffer layout
  uint32_t offset = 0;

  bool operator==(const StructMember& other) const {
    return type == other.type && name == other.name && offset == other.offset;
  }
};

// Type description (stored in TypeTable)
struct Type {
  TypeKind kind = TypeKind::kVoid;

  // For vector: component type and count (2,3,4)
  // For matrix: component type, rows, cols
  // For array: element type, count
  // For pointer: pointee type (storage_class separate)
  TypeId element_type = kInvalidTypeId;
  uint32_t count = 0;   // vector size, array size, or matrix rows
  uint32_t count2 = 0;  // matrix columns

  // For struct
  std::vector<StructMember> members;

  // For pointer
  StorageClass storage_class = StorageClass::kFunction;

  // Equality for deduplication
  bool operator==(const Type& other) const {
    return kind == other.kind && element_type == other.element_type &&
           count == other.count && count2 == other.count2 &&
           members == other.members && storage_class == other.storage_class;
  }
};

// Hash support for Type
struct TypeHash {
  size_t operator()(const Type& type) const;
};

// Type table: manages type creation and deduplication
class TypeTable {
 public:
  TypeTable();
  ~TypeTable();

  // Prevent copy/move
  TypeTable(const TypeTable&) = delete;
  TypeTable& operator=(const TypeTable&) = delete;

  // Get primitive types (always valid, never fails)
  TypeId GetVoidType() const { return void_type_id_; }
  TypeId GetBoolType() const { return bool_type_id_; }
  TypeId GetI32Type() const { return i32_type_id_; }
  TypeId GetU32Type() const { return u32_type_id_; }
  TypeId GetF32Type() const { return f32_type_id_; }
  TypeId GetSamplerType() const { return sampler_type_id_; }

  // Get or create composite types
  TypeId GetVectorType(TypeId component_type, uint32_t count);
  TypeId GetMatrixType(TypeId component_type, uint32_t rows, uint32_t cols);
  TypeId GetArrayType(TypeId element_type, uint32_t count);
  TypeId GetStructType(const std::vector<StructMember>& members);
  TypeId GetPointerType(TypeId pointee, StorageClass storage);
  TypeId GetTexture2DType(TypeId sampled_type);

  // Lookup type info by ID
  const Type* GetType(TypeId id) const;

  // Get the size of the type table (for ID bound calculation)
  size_t Size() const { return types_.size(); }

  // Check if a type is valid
  bool IsValidType(TypeId id) const {
    // IDs are 1-based
    return id != kInvalidTypeId && id <= types_.size();
  }

  // Check type properties
  bool IsScalarType(TypeId id) const;
  bool IsIntegerType(TypeId id) const;
  bool IsFloatType(TypeId id) const;
  bool IsVectorType(TypeId id) const;
  bool IsMatrixType(TypeId id) const;
  bool IsArrayType(TypeId id) const;
  bool IsSamplerType(TypeId id) const;
  bool IsTextureType(TypeId id) const;

  // Get component count for vectors
  uint32_t GetVectorComponentCount(TypeId id) const;

  // Get component type for vectors/matrices
  TypeId GetComponentType(TypeId id) const;
  TypeId GetIndexedElementType(TypeId id);

  // Layout calculation for uniform/storage buffers
  // std140 is used for uniform buffers, std430 for storage buffers
  enum class LayoutRule {
    kStd140,  // Uniform buffer layout
    kStd430,  // Storage buffer layout
  };

  // Calculate size and alignment of a type
  struct LayoutInfo {
    uint32_t size = 0;
    uint32_t alignment = 1;
  };
  LayoutInfo GetLayoutInfo(TypeId id, LayoutRule rule) const;

  // Align an offset to a given alignment
  static uint32_t AlignOffset(uint32_t offset, uint32_t alignment);

 private:
  TypeId RegisterType(Type type);

  std::vector<std::unique_ptr<Type>> types_;
  std::unordered_map<Type, TypeId, TypeHash> type_map_;

  // Cached primitive type IDs
  TypeId void_type_id_ = kInvalidTypeId;
  TypeId bool_type_id_ = kInvalidTypeId;
  TypeId i32_type_id_ = kInvalidTypeId;
  TypeId u32_type_id_ = kInvalidTypeId;
  TypeId f32_type_id_ = kInvalidTypeId;
  TypeId sampler_type_id_ = kInvalidTypeId;
};

}  // namespace ir
}  // namespace wgx
