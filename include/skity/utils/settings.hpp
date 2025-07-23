// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_UTILS_SETTINGS_HPP
#define INCLUDE_SKITY_UTILS_SETTINGS_HPP

#include <atomic>
#include <skity/macros.hpp>

namespace skity {

/**
 * Skity's global settings object generally sets properties that are not related
 * to drawing. All the functions in this class is thread-safe and idempotent.
 */
class SKITY_API Settings {
 public:
  static Settings& GetSettings();

  bool EnableThemeFont() const;
  void SetEnableThemeFont(bool enable);

 private:
  std::atomic<bool> enable_theme_font_{false};
};

}  // namespace skity

#endif  // INCLUDE_SKITY_UTILS_SETTINGS_HPP
