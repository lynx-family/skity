// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <array>
#include <cstring>

#include "wgsl_cross.h"

namespace wgx {

namespace ast {

struct Var;
struct Type;

}  // namespace ast

class Function;

enum class MemoryLayout {
  kStd140,
  kStd430,
  kStd430MSL,
};

std::unique_ptr<TypeDefinition> CreateTypeDefinition(const ast::Type& type,
                                                     Function* func,
                                                     MemoryLayout layout);

template <typename T, size_t ALIGN>
struct Primitive : public TypeDefinition {
  static constexpr size_t TYPE_ALIGN = ALIGN;

  T value = {};

  Primitive(const char* name) : TypeDefinition(name, sizeof(T), ALIGN) {}

  bool SetData(const void* data, size_t size) override {
    value = *(T*)data;
    return true;
  }

  void WriteToBuffer(void* buffer, size_t offset) const override {
    *(T*)((uint8_t*)buffer + offset) = value;
  }
};

struct F32 : public Primitive<float, 4> {
  constexpr static const char* NAME = "f32";

  F32() : Primitive(NAME) {}
};

struct F16 : public Primitive<uint16_t, 2> {
  constexpr static const char* NAME = "f16";

  F16() : Primitive(NAME) {}

  bool SetData(const void* data, size_t size) override {
    if (size != sizeof(uint16_t)) {
      return false;
    }
    value = *(uint16_t*)data;
    return true;
  }
};

struct I32 : public Primitive<int32_t, 4> {
  constexpr static const char* NAME = "i32";

  I32() : Primitive(NAME) {}
};

struct U32 : public Primitive<uint32_t, 4> {
  constexpr static const char* NAME = "u32";

  U32() : Primitive(NAME) {}
};

struct Bool : public Primitive<bool, 4> {
  constexpr static const char* NAME = "bool";

  Bool() : Primitive(NAME) {}
};

struct Vec2F32 : public Primitive<std::array<float, 2>, 8> {
  constexpr static const char* NAME = "vec2<f32>";

  Vec2F32() : Primitive(NAME) {}
};

struct Vec3F32 : public Primitive<std::array<float, 3>, 16> {
  Vec3F32() : Primitive("vec3<f32>") {}
};

struct Vec3F32MSL : public Primitive<std::array<float, 4>, 16> {
  Vec3F32MSL() : Primitive("vec3<f32>") {}

  bool SetData(const void* data, size_t size) override {
    std::memcpy(this->value.data(), data, size);
    return true;
  }
};

struct Vec4F32 : public Primitive<std::array<float, 4>, 16> {
  Vec4F32() : Primitive("vec4<f32>") {}
};

struct Vec2I32 : public Primitive<std::array<int32_t, 2>, 8> {
  Vec2I32() : Primitive("vec2<i32>") {}
};

struct Vec3I32 : public Primitive<std::array<int32_t, 3>, 16> {
  Vec3I32() : Primitive("vec3<i32>") {}
};

struct Vec3I32MSL : public Primitive<std::array<int32_t, 4>, 16> {
  Vec3I32MSL() : Primitive("vec3<i32>") {}

  bool SetData(const void* data, size_t size) override {
    std::memcpy(this->value.data(), data, size);
    return true;
  }
};

struct Vec4I32 : public Primitive<std::array<int32_t, 4>, 16> {
  Vec4I32() : Primitive("vec4<i32>") {}
};

struct Vec2U32 : public Primitive<std::array<uint32_t, 2>, 8> {
  Vec2U32() : Primitive("vec2<u32>") {}
};

struct Vec3U32 : public Primitive<std::array<uint32_t, 3>, 16> {
  Vec3U32() : Primitive("vec3<u32>") {}
};

struct Vec3U32MSL : public Primitive<std::array<uint32_t, 4>, 16> {
  Vec3U32MSL() : Primitive("vec3<u32>") {}

  bool SetData(const void* data, size_t size) override {
    std::memcpy(this->value.data(), data, size);
    return true;
  }
};

struct Vec4U32 : public Primitive<std::array<uint32_t, 4>, 16> {
  Vec4U32() : Primitive("vec4<u32>") {}
};

struct Mat4x4F32 : public Primitive<std::array<float, 16>, 16> {
  Mat4x4F32() : Primitive("mat4x4<f32>") {}
};

struct Mat3x3F32 : public Primitive<std::array<float, 12>, 16> {
  Mat3x3F32() : Primitive("mat3x3<f32>") {}
};

struct Mat2x2F32 : public Primitive<std::array<float, 4>, 8> {
  Mat2x2F32() : Primitive("mat2x2<f32>") {}
};

struct CommonArray : public ArrayDefinition {
  CommonArray(std::vector<std::unique_ptr<TypeDefinition>> elements,
              MemoryLayout layout);

  ~CommonArray() override = default;

  TypeDefinition* GetElementAt(uint32_t index) override;

  bool SetData(const void* data, size_t size) override;

  void WriteToBuffer(void* buffer, size_t offset) const override;

 private:
  std::vector<std::unique_ptr<TypeDefinition>> elements_;
  size_t size_of_element_ = 0;
  size_t stride_of_element_ = 0;
};

}  // namespace wgx
