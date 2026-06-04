// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/case/validation.hpp"

#include <cctype>
#include <utility>

namespace skity {
namespace font_harness {

void ValidationContext::AddError(std::string path, std::string message) {
  errors_.push_back({std::move(path), std::move(message)});
}

Json::Value ValidationContext::ToJson() const {
  Json::Value errors(Json::arrayValue);
  for (const auto& error : errors_) {
    Json::Value item(Json::objectValue);
    item["path"] = error.path;
    item["message"] = error.message;
    errors.append(std::move(item));
  }
  return errors;
}

std::string ChildPath(const std::string& path, const std::string& field) {
  if (path.empty() || path == "$") {
    return "$." + field;
  }
  return path + "." + field;
}

std::string IndexPath(const std::string& path, Json::ArrayIndex index) {
  return path + "[" + std::to_string(index) + "]";
}

bool RequireObject(const Json::Value& value, const std::string& path,
                   ValidationContext* context) {
  if (value.isObject()) {
    return true;
  }
  context->AddError(path, "expected object");
  return false;
}

bool RequireArrayField(const Json::Value& value, const std::string& field,
                       const std::string& path, ValidationContext* context,
                       const Json::Value** out) {
  const std::string field_path = ChildPath(path, field);
  if (!value.isMember(field)) {
    context->AddError(field_path, "required field is missing");
    return false;
  }
  const Json::Value& member = value[field];
  if (!member.isArray()) {
    context->AddError(field_path, "expected array");
    return false;
  }
  if (out != nullptr) {
    *out = &member;
  }
  return true;
}

bool RequireObjectField(const Json::Value& value, const std::string& field,
                        const std::string& path, ValidationContext* context,
                        const Json::Value** out) {
  const std::string field_path = ChildPath(path, field);
  if (!value.isMember(field)) {
    context->AddError(field_path, "required field is missing");
    return false;
  }
  const Json::Value& member = value[field];
  if (!member.isObject()) {
    context->AddError(field_path, "expected object");
    return false;
  }
  if (out != nullptr) {
    *out = &member;
  }
  return true;
}

bool RequireStringField(const Json::Value& value, const std::string& field,
                        const std::string& path, ValidationContext* context,
                        std::string* out) {
  const std::string field_path = ChildPath(path, field);
  if (!value.isMember(field)) {
    context->AddError(field_path, "required field is missing");
    return false;
  }
  const Json::Value& member = value[field];
  if (!member.isString()) {
    context->AddError(field_path, "expected string");
    return false;
  }
  if (out != nullptr) {
    *out = member.asString();
  }
  return true;
}

bool RequireIntField(const Json::Value& value, const std::string& field,
                     const std::string& path, ValidationContext* context,
                     int* out) {
  const std::string field_path = ChildPath(path, field);
  if (!value.isMember(field)) {
    context->AddError(field_path, "required field is missing");
    return false;
  }
  const Json::Value& member = value[field];
  if (!member.isInt()) {
    context->AddError(field_path, "expected integer");
    return false;
  }
  if (out != nullptr) {
    *out = member.asInt();
  }
  return true;
}

bool OptionalStringField(const Json::Value& value, const std::string& field,
                         const std::string& path, ValidationContext* context,
                         std::string* out) {
  if (!value.isMember(field)) {
    return false;
  }
  const std::string field_path = ChildPath(path, field);
  const Json::Value& member = value[field];
  if (!member.isString()) {
    context->AddError(field_path, "expected string");
    return false;
  }
  if (out != nullptr) {
    *out = member.asString();
  }
  return true;
}

bool OptionalIntField(const Json::Value& value, const std::string& field,
                      const std::string& path, ValidationContext* context,
                      int* out) {
  if (!value.isMember(field)) {
    return false;
  }
  const std::string field_path = ChildPath(path, field);
  const Json::Value& member = value[field];
  if (!member.isInt()) {
    context->AddError(field_path, "expected integer");
    return false;
  }
  if (out != nullptr) {
    *out = member.asInt();
  }
  return true;
}

bool OptionalNumberField(const Json::Value& value, const std::string& field,
                         const std::string& path, ValidationContext* context,
                         double* out) {
  if (!value.isMember(field)) {
    return false;
  }
  const std::string field_path = ChildPath(path, field);
  const Json::Value& member = value[field];
  if (!member.isNumeric()) {
    context->AddError(field_path, "expected number");
    return false;
  }
  if (out != nullptr) {
    *out = member.asDouble();
  }
  return true;
}

bool OptionalBoolField(const Json::Value& value, const std::string& field,
                       const std::string& path, ValidationContext* context,
                       bool* out) {
  if (!value.isMember(field)) {
    return false;
  }
  const std::string field_path = ChildPath(path, field);
  const Json::Value& member = value[field];
  if (!member.isBool()) {
    context->AddError(field_path, "expected bool");
    return false;
  }
  if (out != nullptr) {
    *out = member.asBool();
  }
  return true;
}

bool IsOneOf(const std::string& value,
             const std::vector<std::string>& allowed_values) {
  for (const auto& allowed_value : allowed_values) {
    if (value == allowed_value) {
      return true;
    }
  }
  return false;
}

bool ValidateStringEnum(const std::string& value, const std::string& path,
                        const std::vector<std::string>& allowed_values,
                        ValidationContext* context) {
  if (IsOneOf(value, allowed_values)) {
    return true;
  }
  context->AddError(path, "unknown enum value: " + value);
  return false;
}

bool ValidateNonEmptyString(const std::string& value, const std::string& path,
                            ValidationContext* context) {
  if (!value.empty()) {
    return true;
  }
  context->AddError(path, "string must not be empty");
  return false;
}

bool ValidateNonNegativeInt(int value, const std::string& path,
                            ValidationContext* context) {
  if (value >= 0) {
    return true;
  }
  context->AddError(path, "integer must be non-negative");
  return false;
}

bool ValidatePositiveNumber(double value, const std::string& path,
                            ValidationContext* context) {
  if (value > 0.0) {
    return true;
  }
  context->AddError(path, "number must be positive");
  return false;
}

bool ValidateSha256String(const std::string& value, const std::string& path,
                          ValidationContext* context) {
  if (value.size() != 64) {
    context->AddError(path, "sha256 must be 64 hex characters");
    return false;
  }
  for (char c : value) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      context->AddError(path, "sha256 must be 64 hex characters");
      return false;
    }
  }
  return true;
}

}  // namespace font_harness
}  // namespace skity
