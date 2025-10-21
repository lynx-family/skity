// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_FONT_DESCRIPTOR_HPP
#define INCLUDE_SKITY_TEXT_FONT_DESCRIPTOR_HPP

#include <skity/macros.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_style.hpp>
#include <string>

namespace skity {

/**
 * FontDescriptor is used to hold the information when regist a Typeface
 * in FontManager.
 *
 * This struct is used to serialize/deserialize a Typeface.
 */
struct SKITY_API FontDescriptor {
  FontStyle style = FontStyle::Normal();

  std::string family_name = {};
  std::string full_name = {};
  std::string post_script_name = {};

  // ttc index
  int32_t collection_index = 0;

  VariationPosition variation_position = {};

  FourByteTag factory_id = {};
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_FONT_DESCRIPTOR_HPP
