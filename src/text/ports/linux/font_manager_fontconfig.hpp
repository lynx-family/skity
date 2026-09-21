// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_LINUX_FONT_MANAGER_FONTCONFIG_HPP
#define SRC_TEXT_PORTS_LINUX_FONT_MANAGER_FONTCONFIG_HPP

#include <memory>
#include <skity/text/font_manager.hpp>
#include <string>
#include <vector>

namespace skity {

// Internal factory. A null path uses FONTCONFIG_FILE or system configuration.
// An explicitly selected configuration must load successfully; otherwise null
// is returned. No process-global Fontconfig configuration is modified.
std::shared_ptr<FontManager> MakeFontManagerFontConfig(
    const char* config_file = nullptr);

struct FontConfigInfo {
  bool initialized = false;
  int runtime_version = 0;
  std::vector<std::string> config_files;
  std::vector<std::string> font_files;
};

FontConfigInfo GetDefaultFontConfigInfo();

}  // namespace skity

#endif  // SRC_TEXT_PORTS_LINUX_FONT_MANAGER_FONTCONFIG_HPP
