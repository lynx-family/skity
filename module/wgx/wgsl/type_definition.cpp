// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl/type_definition.h"

#include "wgsl/ast/expression.h"
#include "wgsl/ast/type_decl.h"
#include "wgsl/ast/variable.h"
#include "wgsl/function.h"

namespace wgx {

std::unique_ptr<TypeDefinition> CreateTypeDefinition(const ast::Type& type,
                                                     Function* func,
                                                     MemoryLayout layout) {
  if (type.IsBuiltin()) {  // primitive type
    const auto& name = type.expr->ident->name;
    if (name == "f32") {
      return std::make_unique<F32>();
    } else if (name == "i32") {
      return std::make_unique<I32>();
    } else if (name == "u32") {
      return std::make_unique<U32>();
    } else if (name == "bool") {
      return std::make_unique<Bool>();
    } else if (name == "vec2") {
      const auto& args = type.expr->ident->args;
      if (args.empty()) {
        return {};
      }

      const auto& type_name =
          static_cast<ast::IdentifierExp*>(args[0])->ident->name;
      if (type_name == "f32") {
        return std::make_unique<Vec2F32>();
      } else if (type_name == "i32") {
        return std::make_unique<Vec2I32>();
      } else if (type_name == "u32") {
        return std::make_unique<Vec2U32>();
      }
    } else if (name == "vec3") {
      const auto& args = type.expr->ident->args;
      if (args.empty()) {
        return {};
      }

      const auto& type_name =
          static_cast<ast::IdentifierExp*>(args[0])->ident->name;
      if (type_name == "f32") {
        if (layout == MemoryLayout::kStd430MSL) {
          return std::make_unique<Vec3F32MSL>();
        } else {
          return std::make_unique<Vec3F32>();
        }
      } else if (type_name == "i32") {
        if (layout == MemoryLayout::kStd430MSL) {
          return std::make_unique<Vec3I32MSL>();
        } else {
          return std::make_unique<Vec3I32>();
        }
      } else if (type_name == "u32") {
        if (layout == MemoryLayout::kStd430MSL) {
          return std::make_unique<Vec3U32MSL>();
        } else {
          return std::make_unique<Vec3U32>();
        }
      }
    } else if (name == "vec4") {
      const auto& args = type.expr->ident->args;
      if (args.empty()) {
        return {};
      }

      const auto& type_name =
          static_cast<ast::IdentifierExp*>(args[0])->ident->name;
      if (type_name == "f32") {
        return std::make_unique<Vec4F32>();
      } else if (type_name == "i32") {
        return std::make_unique<Vec4I32>();
      } else if (type_name == "u32") {
        return std::make_unique<Vec4U32>();
      }
    } else if (name == "mat2x2") {
      return std::make_unique<Mat2x2F32>();
    } else if (name == "mat3x3") {
      return std::make_unique<Mat3x3F32>();
    } else if (name == "mat4x4") {
      return std::make_unique<Mat4x4F32>();
    }
  } else if (type.IsArray()) {
    const auto& args = type.expr->ident->args;
    if (args.size() < 2) {
      return {};
    }

    auto element_type = args[0];
    auto element_count = args[1];

    if (element_type->GetType() != ast::ExpressionType::kIdentifier ||
        element_count->GetType() != ast::ExpressionType::kIntLiteral) {
      return {};
    }

    size_t count = static_cast<ast::IntLiteralExp*>(element_count)->value;

    std::vector<std::unique_ptr<TypeDefinition>> elements{count};

    ast::Type et{static_cast<ast::IdentifierExp*>(element_type)};

    for (size_t i = 0; i < count; ++i) {
      elements[i] = CreateTypeDefinition(et, func, layout);
    }

    return std::make_unique<CommonArray>(std::move(elements), layout);
  } else {
    const auto& type_name = type.expr->ident->name;

    auto decl = func->GetTypeDecl(type_name);

    if (!decl || decl->GetType() != ast::TypeDeclType::kStruct) {
      return {};
    }

    auto* struct_decl = static_cast<ast::StructDecl*>(decl);

    std::vector<Field*> members{};

    for (auto* member : struct_decl->members) {
      const auto& member_name = member->name->name;
      auto member_decl = func->GetTypeDecl(member_name);

      std::unique_ptr<TypeDefinition> member_def;

      if (member_decl) {
        ast::IdentifierExp exp{member_decl->name};

        ast::Type member_type{&exp};

        member_def = CreateTypeDefinition(member_type, func, layout);
      } else {
        // this is a builtin type
        auto const& member_type = member->type;

        member_def = CreateTypeDefinition(member_type, func, layout);
      }

      if (!member_def) {
        continue;
      }

      if (member->GetAttribute(ast::AttributeType::kAlign) != nullptr) {
        auto* align_attr = static_cast<ast::AlignAttribute*>(
            member->GetAttribute(ast::AttributeType::kAlign));

        member_def->alignment = align_attr->offset;
      }

      auto member_field = new Field{member_name, member_def.release()};

      members.push_back(member_field);
    }

    if (members.empty()) {
      return {};
    }

    return std::make_unique<StructDefinition>(type_name, members);
  }

  return {};
}

/**
 * calculate the struct alignment based on std140 layout rules
 * @ref https://www.w3.org/TR/WGSL/#alignment-and-size
 *
 * AlignOf(S) = max(AlignOf(S.m0), ..., AlignOf(S.mN))
 */
static size_t calculate_alignment(const std::vector<Field*>& members) {
  size_t align = 0;

  for (auto* member : members) {
    if (member->type->alignment > align) {
      align = member->type->alignment;
    }
  }

  return align;
}

/// return ⌈n ÷ k⌉ × k
size_t round_up(size_t k, size_t n) { return (n + k - 1) / k * k; }

StructDefinition::StructDefinition(const std::string_view& name,
                                   std::vector<Field*> members)
    : TypeDefinition(name, 0, calculate_alignment(members)),
      members(std::move(members)) {
  size_t offset = 0;
  for (auto* member : this->members) {
    offset = round_up(member->type->alignment, offset);

    member->offset = offset;

    offset += member->type->size;
  }

  this->size = round_up(this->alignment, offset);
}

bool StructDefinition::SetData(const void* data, size_t size) {
  if (size != this->size) {
    return false;
  }

  for (auto* member : members) {
    member->type->SetData((uint8_t*)data + member->offset, member->type->size);
  }

  return true;
}

void StructDefinition::WriteToBuffer(void* buffer, size_t offset) const {
  for (auto* member : members) {
    member->type->WriteToBuffer(buffer, offset + member->offset);
  }
}

Field* StructDefinition::GetMember(const std::string_view& name) {
  for (auto* member : members) {
    if (member->name == name) {
      return member;
    }
  }

  return nullptr;
}

CommonArray::CommonArray(std::vector<std::unique_ptr<TypeDefinition>> elements,
                         MemoryLayout layout)
    : ArrayDefinition(), elements_(std::move(elements)) {
  this->count = elements_.size();

  if (this->count == 0) {
    return;
  }

  size_of_element_ = elements_[0]->size;
  size_t align_of_element = elements_[0]->alignment;
  size_t align_of_array = layout == MemoryLayout::kStd140
                              ? round_up(16, align_of_element)
                              : align_of_element;

  stride_of_element_ = round_up(align_of_array, size_of_element_);

  this->size = stride_of_element_ * this->count;
  this->alignment = align_of_array;

  this->name = "array<";
  this->name += elements_[0]->name;
  this->name += ", ";
  this->name += std::to_string(this->count);
  this->name += ">";
}

TypeDefinition* CommonArray::GetElementAt(uint32_t index) {
  if (index >= count) {
    return nullptr;
  }

  return elements_[index].get();
}

bool CommonArray::SetData(const void* data, size_t size) {
  if (size != this->size) {
    return false;
  }

  for (uint32_t i = 0; i < count; i++) {
    if (!elements_[i]->SetData((uint8_t*)data + i * stride_of_element_,
                               size_of_element_)) {
      return false;
    }
  }

  return true;
}

void CommonArray::WriteToBuffer(void* buffer, size_t offset) const {
  for (uint32_t i = 0; i < count; i++) {
    elements_[i]->WriteToBuffer(buffer, offset + i * stride_of_element_);
  }
}

}  // namespace wgx
