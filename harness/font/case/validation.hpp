// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_CASE_VALIDATION_HPP
#define HARNESS_FONT_CASE_VALIDATION_HPP

#include <string>
#include <vector>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

struct ValidationError {
  std::string path;
  std::string message;
};

class ValidationContext {
 public:
  void AddError(std::string path, std::string message);

  bool IsValid() const { return errors_.empty(); }
  const std::vector<ValidationError>& Errors() const { return errors_; }
  Json::Value ToJson() const;

 private:
  std::vector<ValidationError> errors_;
};

std::string ChildPath(const std::string& path, const std::string& field);
std::string IndexPath(const std::string& path, Json::ArrayIndex index);

bool RequireObject(const Json::Value& value, const std::string& path,
                   ValidationContext* context);
bool RequireArrayField(const Json::Value& value, const std::string& field,
                       const std::string& path, ValidationContext* context,
                       const Json::Value** out = nullptr);
bool RequireObjectField(const Json::Value& value, const std::string& field,
                        const std::string& path, ValidationContext* context,
                        const Json::Value** out = nullptr);
bool RequireStringField(const Json::Value& value, const std::string& field,
                        const std::string& path, ValidationContext* context,
                        std::string* out = nullptr);
bool RequireIntField(const Json::Value& value, const std::string& field,
                     const std::string& path, ValidationContext* context,
                     int* out = nullptr);
bool OptionalStringField(const Json::Value& value, const std::string& field,
                         const std::string& path, ValidationContext* context,
                         std::string* out = nullptr);
bool OptionalIntField(const Json::Value& value, const std::string& field,
                      const std::string& path, ValidationContext* context,
                      int* out = nullptr);
bool OptionalNumberField(const Json::Value& value, const std::string& field,
                         const std::string& path, ValidationContext* context,
                         double* out = nullptr);
bool OptionalBoolField(const Json::Value& value, const std::string& field,
                       const std::string& path, ValidationContext* context,
                       bool* out = nullptr);

bool IsOneOf(const std::string& value,
             const std::vector<std::string>& allowed_values);
bool ValidateStringEnum(const std::string& value, const std::string& path,
                        const std::vector<std::string>& allowed_values,
                        ValidationContext* context);
bool ValidateNonEmptyString(const std::string& value, const std::string& path,
                            ValidationContext* context);
bool ValidateNonNegativeInt(int value, const std::string& path,
                            ValidationContext* context);
bool ValidatePositiveNumber(double value, const std::string& path,
                            ValidationContext* context);
bool ValidateSha256String(const std::string& value, const std::string& path,
                          ValidationContext* context);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_CASE_VALIDATION_HPP
