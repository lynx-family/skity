// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PROBE_TYPEFACE_IDENTITY_HPP
#define HARNESS_FONT_PROBE_TYPEFACE_IDENTITY_HPP

#include <memory>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_descriptor.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/typeface.hpp>
#include <string>
#include <vector>

#include "third_party/jsoncpp/include/json/json.h"

namespace skity {
namespace font_harness {

std::string FontHarnessTagToString(uint32_t tag);

Json::Value FontStyleToIdentityJson(const FontStyle& style);

Json::Value TypefaceVariationPositionToJson(const VariationPosition& position);

Json::Value TypefaceVariationAxesToJson(std::vector<VariationAxis> axes);

Json::Value TypefaceIdentityToJson(const std::shared_ptr<Typeface>& typeface,
                                   const FontDescriptor& descriptor);

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PROBE_TYPEFACE_IDENTITY_HPP
