// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/case/platform_target.hpp"

namespace skity {
namespace font_harness {

namespace {

struct PlatformAliasEntry {
  const char* alias;
  const char* canonical_id;
};

const std::vector<PlatformTargetInfo>& PlatformTargetRegistry() {
  static const std::vector<PlatformTargetInfo> entries = {
      {"macos-coretext", "coretext", "host-cli"},
      {"ios-sim-coretext", "coretext", "ios-xctest"},
      {"ios-device-coretext", "coretext", "ios-xctest"},
      {"android-freetype", "freetype", "android-adb"},
      {"windows-directwrite", "directwrite", "windows-host"},
      {"host-ft", "host-ft", "host-cli"},
  };
  return entries;
}

const std::vector<PlatformAliasEntry>& PlatformTargetAliases() {
  static const std::vector<PlatformAliasEntry> aliases = {
      {"darwin-ct", "macos-coretext"},
      {"windows-dwrite", "windows-directwrite"},
  };
  return aliases;
}

}  // namespace

const std::vector<std::string>& AllowedBackendIds() {
  static const std::vector<std::string> values = {"coretext", "freetype",
                                                  "directwrite", "host-ft"};
  return values;
}

std::string CanonicalPlatformTarget(const std::string& platform) {
  for (const auto& alias : PlatformTargetAliases()) {
    if (platform == alias.alias) {
      return alias.canonical_id;
    }
  }
  return platform;
}

const PlatformTargetInfo* FindPlatformTargetInfo(const std::string& platform) {
  const std::string canonical = CanonicalPlatformTarget(platform);
  for (const auto& entry : PlatformTargetRegistry()) {
    if (canonical == entry.id) {
      return &entry;
    }
  }
  return nullptr;
}

bool PlatformTargetMatchesBackend(const std::string& backend,
                                  const std::string& platform) {
  const PlatformTargetInfo* info = FindPlatformTargetInfo(platform);
  return info != nullptr && info->backend == backend;
}

bool PlatformArrayMatchesBackend(const std::string& backend,
                                 const Json::Value& platforms) {
  if (!platforms.isArray()) {
    return false;
  }
  for (const auto& platform : platforms) {
    if (!platform.isString()) {
      continue;
    }
    if (PlatformTargetMatchesBackend(backend, platform.asString())) {
      return true;
    }
  }
  return false;
}

bool PlatformArrayContainsTarget(const Json::Value& platforms,
                                 const std::string& target_platform) {
  if (!platforms.isArray()) {
    return false;
  }
  const std::string canonical_target = CanonicalPlatformTarget(target_platform);
  for (const auto& platform : platforms) {
    if (!platform.isString()) {
      continue;
    }
    if (CanonicalPlatformTarget(platform.asString()) == canonical_target) {
      return true;
    }
  }
  return false;
}

std::string InferTargetPlatformFromArray(const Json::Value& platforms,
                                         const std::string& backend) {
  if (!platforms.isArray()) {
    return "";
  }
  for (const auto& platform : platforms) {
    if (!platform.isString()) {
      continue;
    }
    if (PlatformTargetMatchesBackend(backend, platform.asString())) {
      return CanonicalPlatformTarget(platform.asString());
    }
  }
  return "";
}

}  // namespace font_harness
}  // namespace skity
