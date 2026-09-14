// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_ARTIFACT_PROVENANCE_HPP
#define HARNESS_FONT_ARTIFACT_PROVENANCE_HPP

#include "harness/font/artifact/fingerprint.hpp"

namespace skity {
namespace font_harness {

std::string CaseProfile(const Json::Value& input);
void AttachProbeInputs(const CaseValidationResult& input,
                       const std::filesystem::path& environment,
                       Json::Value* artifact, ValidationContext* errors);
void ValidateArtifactInputs(const CaseValidationResult& input,
                            const Json::Value& artifact,
                            const std::filesystem::path& artifact_path,
                            const std::filesystem::path& environment,
                            bool oracle, ValidationContext* errors);

}  // namespace font_harness
}  // namespace skity
#endif  // HARNESS_FONT_ARTIFACT_PROVENANCE_HPP
