// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_ARTIFACT_FINGERPRINT_HPP
#define HARNESS_FONT_ARTIFACT_FINGERPRINT_HPP

#include <filesystem>
#include <string>
#include <string_view>

#include "harness/font/case/case_document.hpp"

namespace skity {
namespace font_harness {

std::string Sha256(std::string_view bytes);
bool FileSha256(const std::filesystem::path& path, std::string* digest,
                std::string* error);
// case-json-v1: typed, length-delimited UTF-8 values, sorted object keys;
// numbers use their big-endian IEEE-754 binary64 representation.
std::string JsonFingerprint(const Json::Value& value);
Json::Value BuildInputFingerprint(const CaseValidationResult& input,
                                  ValidationContext* errors);

}  // namespace font_harness
}  // namespace skity
#endif  // HARNESS_FONT_ARTIFACT_FINGERPRINT_HPP
