// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/platform/linux/env_info.hpp"

#include <algorithm>
#include <cstdlib>
#include <skity/text/font_manager.hpp>
#include <vector>

#include "harness/font/probe/backend_support.hpp"

#if SKITY_FONT_HARNESS_HAS_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include <sys/utsname.h>
#endif

namespace skity {
namespace font_harness {

Json::Value BuildLinuxEnvInfo(const std::filesystem::path& repo_root,
                              const std::string& backend, bool list_fonts) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = list_fonts ? "font_list_fonts" : "font_env_info";
  report["backend"] = backend;
  report["target_platform"] = "linux-" + backend;
  const bool available = backend == "freetype"
                             ? IsExplicitSourceProbeBackendAvailable(backend)
                             : IsHostFontProbeBackendAvailable(backend);
  report["backend_available"] = available;
  report["ok"] = available;
  report["reason_code"] = available ? "ok" : "backend_unavailable";
#if SKITY_FONT_HARNESS_HAS_FONTCONFIG
  report["font_manager"] = "fontconfig";
#else
  report["font_manager"] = "test_fixture";
#endif
  report["capabilities"]["explicit_typeface"] = available;
  report["capabilities"]["system_font_matching"] =
      backend == "fontconfig" && available;
  report["font_inventory_scope"] =
      backend == "fontconfig"
          ? (std::getenv("FONTCONFIG_FILE") ? "configured_fonts"
                                            : "system_fonts")
          : "repository_fixture_files";
#if SKITY_FONT_HARNESS_HAS_FONTCONFIG
  if (backend == "fontconfig") {
    const auto info = GetDefaultFontConfigInfo();
    auto& inventory = report["fontconfig_inventory"];
    inventory["version"] = info.runtime_version;
    inventory["initialized"] = info.initialized;
    inventory["files"] = Json::Value(Json::arrayValue);
    inventory["config_files"] = Json::Value(Json::arrayValue);
    for (const auto& file : info.font_files) {
      inventory["files"].append(file);
    }
    for (const auto& file : info.config_files) {
      inventory["config_files"].append(file);
    }
    if (list_fonts) {
      auto manager = FontManager::RefDefault();
      report["families"] = Json::Value(Json::arrayValue);
      for (int i = 0; i < manager->CountFamilies(); ++i) {
        report["families"].append(manager->GetFamilyName(i));
      }
      report["family_count"] = manager->CountFamilies();
    }
  }
#endif
  report["repo_root"] = repo_root.string();
  for (const char* name : {"LANG", "LC_ALL", "LC_CTYPE", "FONTCONFIG_FILE",
                           "FONTCONFIG_PATH", "FONTCONFIG_SYSROOT"}) {
    const char* value = std::getenv(name);
    report["environment"][name] = value ? Json::Value(value) : Json::Value();
  }
#if SKITY_FONT_HARNESS_HAS_FREETYPE
  struct utsname os {};
  if (uname(&os) == 0) {
    report["os"]["system"] = os.sysname;
    report["os"]["release"] = os.release;
    report["os"]["machine"] = os.machine;
    const std::string release(os.release);
    report["os"]["wsl"] = release.find("microsoft") != std::string::npos ||
                          release.find("WSL") != std::string::npos;
  }
  report["freetype"]["source"] = "skity bundled freetype2 target";
  report["freetype"]["build_version"] = std::to_string(FREETYPE_MAJOR) + "." +
                                        std::to_string(FREETYPE_MINOR) + "." +
                                        std::to_string(FREETYPE_PATCH);
  FT_Library library = nullptr;
  if (FT_Init_FreeType(&library) == 0) {
    FT_Int major, minor, patch;
    FT_Library_Version(library, &major, &minor, &patch);
    report["freetype"]["runtime_version"] = std::to_string(major) + "." +
                                            std::to_string(minor) + "." +
                                            std::to_string(patch);
    FT_Done_FreeType(library);
  }
#endif
  if (!available) {
    report["error"] =
        backend == "fontconfig"
            ? HostFontBackendUnavailableMessage(backend, "env-info")
            : ExplicitSourceBackendUnavailableMessage(backend, "env-info");
  }
  if (list_fonts && backend == "freetype") {
    report["font_files"] = Json::Value(Json::arrayValue);
    const auto directory = repo_root / "test/fonts/resources";
    std::error_code error;
    std::vector<std::string> files;
    for (const auto& file :
         std::filesystem::directory_iterator(directory, error)) {
      const auto extension = file.path().extension().string();
      if (extension == ".ttf" || extension == ".ttc" || extension == ".otf") {
        files.push_back(file.path().filename().string());
      }
    }
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
      report["font_files"].append("repo://test/fonts/resources/" + file);
    }
    report["font_file_count"] = static_cast<Json::UInt>(files.size());
  }
  return report;
}

}  // namespace font_harness
}  // namespace skity
