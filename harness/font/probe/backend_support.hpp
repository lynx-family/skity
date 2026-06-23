// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_FONT_PROBE_BACKEND_SUPPORT_HPP
#define HARNESS_FONT_PROBE_BACKEND_SUPPORT_HPP

#include <skity/utils/settings.hpp>
#include <string>

#ifndef SKITY_FONT_HARNESS_HAS_CORETEXT
#define SKITY_FONT_HARNESS_HAS_CORETEXT 0
#endif

#ifndef SKITY_FONT_HARNESS_HAS_DIRECTWRITE
#define SKITY_FONT_HARNESS_HAS_DIRECTWRITE 0
#endif

namespace skity {
namespace font_harness {

inline bool IsHostFontProbeBackend(const std::string& backend) {
  return backend == "coretext" || backend == "directwrite";
}

inline bool IsHostFontProbeBackendAvailable(const std::string& backend) {
  if (backend == "coretext") {
    return static_cast<bool>(SKITY_FONT_HARNESS_HAS_CORETEXT);
  }
  if (backend == "directwrite") {
    if (!static_cast<bool>(SKITY_FONT_HARNESS_HAS_DIRECTWRITE)) {
      return false;
    }
#if defined(SKITY_WIN)
    return Settings::GetSettings().EnableDWriteFontManager();
#else
    return false;
#endif
  }
  return false;
}

inline std::string HostFontBackendUnavailableMessage(
    const std::string& backend, const std::string& probe_name) {
  if (backend == "coretext") {
    return "CoreText backend is unavailable; build on macOS with "
           "SKITY_CT_FONT=ON";
  }
  if (backend == "directwrite") {
#if defined(SKITY_WIN)
    if (!Settings::GetSettings().EnableDWriteFontManager()) {
      return "DirectWrite font manager is disabled; enable it before "
             "running the DirectWrite font harness";
    }
#endif
    return "DirectWrite backend is unavailable; build on Windows";
  }
  return probe_name + " supports only the coretext and directwrite backends";
}

inline bool IsExplicitSourceProbeBackend(const std::string& backend) {
  return IsHostFontProbeBackend(backend);
}

inline bool IsExplicitSourceProbeBackendAvailable(const std::string& backend) {
  return IsHostFontProbeBackendAvailable(backend);
}

inline std::string ExplicitSourceBackendUnavailableMessage(
    const std::string& backend, const std::string& probe_name) {
  return HostFontBackendUnavailableMessage(backend, probe_name);
}

}  // namespace font_harness
}  // namespace skity

#endif  // HARNESS_FONT_PROBE_BACKEND_SUPPORT_HPP
