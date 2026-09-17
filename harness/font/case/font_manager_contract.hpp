// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_CASE_FONT_MANAGER_CONTRACT_HPP
#define HARNESS_FONT_CASE_FONT_MANAGER_CONTRACT_HPP

#include "harness/font/case/validation.hpp"

namespace skity {
namespace font_harness {

void ValidateFontManagerCase(const Json::Value& root,
                             ValidationContext* errors);
void ValidateFontManagerResult(const Json::Value& root,
                               const Json::Value& artifact,
                               ValidationContext* errors);

}  // namespace font_harness
}  // namespace skity
#endif  // HARNESS_FONT_CASE_FONT_MANAGER_CONTRACT_HPP
