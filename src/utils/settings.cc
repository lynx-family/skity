// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <memory>
#include <skity/utils/settings.hpp>

namespace skity {

Settings& Settings::GetSettings() {
  static std::unique_ptr<Settings> settings = std::make_unique<Settings>();
  return *settings;
}

bool Settings::EnableThemeFont() const { return enable_theme_font_.load(); }

void Settings::SetEnableThemeFont(bool enable) {
  enable_theme_font_.store(enable);
}

}  // namespace skity
