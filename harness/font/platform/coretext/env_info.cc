// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/platform/coretext/env_info.hpp"

#if !defined(_WIN32)
#include <sys/utsname.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#if SKITY_FONT_HARNESS_HAS_CORETEXT
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#endif

#ifndef SKITY_FONT_HARNESS_CMAKE_SYSTEM_NAME
#define SKITY_FONT_HARNESS_CMAKE_SYSTEM_NAME "unknown"
#endif

#ifndef SKITY_FONT_HARNESS_SKITY_CT_FONT
#define SKITY_FONT_HARNESS_SKITY_CT_FONT 0
#endif

#ifndef SKITY_FONT_HARNESS_ENABLE_FONT_HARNESS
#define SKITY_FONT_HARNESS_ENABLE_FONT_HARNESS 0
#endif

#ifndef SKITY_FONT_HARNESS_HAS_CORETEXT
#define SKITY_FONT_HARNESS_HAS_CORETEXT 0
#endif

#ifndef SKITY_FONT_HARNESS_TARGET_PLATFORM
#define SKITY_FONT_HARNESS_TARGET_PLATFORM "unknown"
#endif

namespace skity {
namespace font_harness {

namespace {

constexpr const char* kBackend = "coretext";
constexpr const char* kTargetPlatform = SKITY_FONT_HARNESS_TARGET_PLATFORM;

std::string Trim(std::string value) {
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' ||
                            value.back() == '\0' || value.back() == ' ' ||
                            value.back() == '\t')) {
    value.pop_back();
  }
  size_t start = 0;
  while (start < value.size() &&
         (value[start] == ' ' || value[start] == '\t')) {
    ++start;
  }
  if (start > 0) {
    value.erase(0, start);
  }
  return value;
}

std::string ReadFirstLine(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::string line;
  if (!std::getline(input, line)) {
    return {};
  }
  return Trim(std::move(line));
}

std::filesystem::path ResolveGitDir(const std::filesystem::path& repo_root) {
  const auto dot_git = repo_root / ".git";
  if (std::filesystem::is_directory(dot_git)) {
    return dot_git;
  }

  const std::string git_file = ReadFirstLine(dot_git);
  constexpr const char* kGitDirPrefix = "gitdir:";
  if (git_file.rfind(kGitDirPrefix, 0) != 0) {
    return {};
  }

  std::filesystem::path git_dir = Trim(git_file.substr(strlen(kGitDirPrefix)));
  if (git_dir.is_absolute()) {
    return git_dir;
  }
  return (repo_root / git_dir).lexically_normal();
}

std::string ReadPackedRef(const std::filesystem::path& git_dir,
                          const std::string& ref_name) {
  std::ifstream input(git_dir / "packed-refs");
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '^') {
      continue;
    }
    std::istringstream stream(line);
    std::string sha;
    std::string ref;
    stream >> sha >> ref;
    if (ref == ref_name) {
      return sha;
    }
  }
  return {};
}

std::string ResolveSkityCommit(const std::filesystem::path& repo_root) {
  const auto git_dir = ResolveGitDir(repo_root);
  if (git_dir.empty()) {
    return {};
  }

  const std::string head = ReadFirstLine(git_dir / "HEAD");
  constexpr const char* kRefPrefix = "ref:";
  if (head.rfind(kRefPrefix, 0) != 0) {
    return head;
  }

  const std::string ref_name = Trim(head.substr(strlen(kRefPrefix)));
  std::string sha = ReadFirstLine(git_dir / ref_name);
  if (!sha.empty()) {
    return sha;
  }
  return ReadPackedRef(git_dir, ref_name);
}

std::string DetectPlatformName() {
  const std::string target_platform = kTargetPlatform;
  if (target_platform.rfind("ios-", 0) == 0) {
    return "iOS";
  }
  if (target_platform.rfind("macos-", 0) == 0) {
    return "macOS";
  }

  const std::string system_name = SKITY_FONT_HARNESS_CMAKE_SYSTEM_NAME;
  if (system_name == "Darwin") {
    return "macOS";
  }
  return system_name.empty() ? "unknown" : system_name;
}

std::string DetectOSVersion() {
#if defined(__APPLE__)
  size_t size = 0;
  if (sysctlbyname("kern.osproductversion", nullptr, &size, nullptr, 0) == 0 &&
      size > 0) {
    std::string version(size, '\0');
    if (sysctlbyname("kern.osproductversion", version.data(), &size, nullptr,
                     0) == 0) {
      return Trim(std::move(version));
    }
  }
#endif

#if !defined(_WIN32)
  struct utsname value;
  if (uname(&value) == 0) {
    return value.release;
  }
#endif
  return "unknown";
}

Json::Value BuildCMakeOptions() {
  Json::Value options(Json::objectValue);
  options["CMAKE_SYSTEM_NAME"] = SKITY_FONT_HARNESS_CMAKE_SYSTEM_NAME;
  options["SKITY_CT_FONT"] =
      static_cast<bool>(SKITY_FONT_HARNESS_SKITY_CT_FONT);
  options["SKITY_ENABLE_FONT_HARNESS"] =
      static_cast<bool>(SKITY_FONT_HARNESS_ENABLE_FONT_HARNESS);
  options["SKITY_FONT_HARNESS_HAS_CORETEXT"] =
      static_cast<bool>(SKITY_FONT_HARNESS_HAS_CORETEXT);
  return options;
}

Json::Value BuildBaseReport(const CoreTextEnvRequest& request,
                            const std::string& artifact_type) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = artifact_type;
  report["platform"] = DetectPlatformName();
  report["platform_id"] = std::string(kTargetPlatform) == "macos-coretext"
                              ? "darwin-ct"
                              : kTargetPlatform;
  report["target_platform"] = kTargetPlatform;
  report["backend"] = kBackend;
  report["os_version"] = DetectOSVersion();
  const std::string commit = ResolveSkityCommit(request.repo_root);
  report["skity_commit"] = commit.empty() ? "unknown" : commit;
  report["cmake_options"] = BuildCMakeOptions();
  report["backend_available"] =
      static_cast<bool>(SKITY_FONT_HARNESS_HAS_CORETEXT);
  if (!static_cast<bool>(SKITY_FONT_HARNESS_HAS_CORETEXT)) {
    report["reason_code"] = "backend_unavailable";
    report["message"] =
        "CoreText backend is unavailable; build on macOS with SKITY_CT_FONT=ON";
  }
  return report;
}

#if SKITY_FONT_HARNESS_HAS_CORETEXT

struct CFReleaseDeleter {
  void operator()(const void* ref) const {
    if (ref != nullptr) {
      CFRelease(ref);
    }
  }
};

template <typename T>
using ScopedCFRef = std::unique_ptr<std::remove_pointer_t<T>, CFReleaseDeleter>;

std::string CFStringToStdString(CFStringRef value) {
  if (value == nullptr) {
    return {};
  }
  CFIndex length = CFStringGetLength(value);
  CFIndex max_size =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::string result(static_cast<size_t>(max_size), '\0');
  if (!CFStringGetCString(value, result.data(), max_size,
                          kCFStringEncodingUTF8)) {
    return {};
  }
  return Trim(std::move(result));
}

ScopedCFRef<CFStringRef> MakeCFString(const std::string& value) {
  return ScopedCFRef<CFStringRef>(CFStringCreateWithCString(
      kCFAllocatorDefault, value.c_str(), kCFStringEncodingUTF8));
}

std::string CopyDescriptorString(CTFontDescriptorRef descriptor,
                                 CFStringRef attribute) {
  ScopedCFRef<CFStringRef> value(static_cast<CFStringRef>(
      CTFontDescriptorCopyAttribute(descriptor, attribute)));
  return CFStringToStdString(value.get());
}

std::vector<std::string> CopyAvailableFamilies() {
  std::vector<std::string> families;
  ScopedCFRef<CFArrayRef> array(CTFontManagerCopyAvailableFontFamilyNames());
  if (!array) {
    return families;
  }

  CFIndex count = CFArrayGetCount(array.get());
  families.reserve(static_cast<size_t>(count));
  for (CFIndex i = 0; i < count; ++i) {
    CFStringRef family =
        static_cast<CFStringRef>(CFArrayGetValueAtIndex(array.get(), i));
    std::string name = CFStringToStdString(family);
    if (!name.empty()) {
      families.push_back(std::move(name));
    }
  }

  std::sort(families.begin(), families.end());
  families.erase(std::unique(families.begin(), families.end()), families.end());
  return families;
}

Json::Value CopyStylesForFamily(const std::string& family_name) {
  Json::Value styles(Json::arrayValue);
  ScopedCFRef<CFStringRef> family = MakeCFString(family_name);
  if (!family) {
    return styles;
  }

  ScopedCFRef<CFMutableDictionaryRef> attributes(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(attributes.get(), kCTFontFamilyNameAttribute,
                       family.get());
  ScopedCFRef<CTFontDescriptorRef> descriptor(
      CTFontDescriptorCreateWithAttributes(attributes.get()));
  if (!descriptor) {
    return styles;
  }

  ScopedCFRef<CFArrayRef> matches(
      CTFontDescriptorCreateMatchingFontDescriptors(descriptor.get(), nullptr));
  if (!matches) {
    return styles;
  }

  std::vector<Json::Value> style_values;
  CFIndex count = CFArrayGetCount(matches.get());
  style_values.reserve(static_cast<size_t>(count));
  for (CFIndex i = 0; i < count; ++i) {
    CTFontDescriptorRef match = static_cast<CTFontDescriptorRef>(
        CFArrayGetValueAtIndex(matches.get(), i));
    Json::Value style(Json::objectValue);
    style["style_name"] =
        CopyDescriptorString(match, kCTFontStyleNameAttribute);
    style["postscript_name"] =
        CopyDescriptorString(match, kCTFontNameAttribute);
    style_values.push_back(std::move(style));
  }

  std::sort(style_values.begin(), style_values.end(),
            [](const Json::Value& lhs, const Json::Value& rhs) {
              return lhs["postscript_name"].asString() <
                     rhs["postscript_name"].asString();
            });
  for (auto& style : style_values) {
    styles.append(std::move(style));
  }
  return styles;
}

bool CanResolveFamilyDescriptor(const std::string& family_name) {
  ScopedCFRef<CFStringRef> family = MakeCFString(family_name);
  if (!family) {
    return false;
  }

  ScopedCFRef<CFMutableDictionaryRef> attributes(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(attributes.get(), kCTFontFamilyNameAttribute,
                       family.get());
  ScopedCFRef<CTFontDescriptorRef> descriptor(
      CTFontDescriptorCreateWithAttributes(attributes.get()));
  if (!descriptor) {
    return false;
  }

  const void* required_values[] = {kCTFontFamilyNameAttribute};
  ScopedCFRef<CFSetRef> required(CFSetCreate(
      kCFAllocatorDefault, required_values, 1, &kCFTypeSetCallBacks));
  ScopedCFRef<CTFontDescriptorRef> resolved(
      CTFontDescriptorCreateMatchingFontDescriptor(descriptor.get(),
                                                   required.get()));
  return resolved != nullptr;
}

#endif

}  // namespace

Json::Value BuildCoreTextEnvInfo(const CoreTextEnvRequest& request) {
  Json::Value report = BuildBaseReport(request, "font_env_info");
  if (!report["backend_available"].asBool()) {
    return report;
  }

#if SKITY_FONT_HARNESS_HAS_CORETEXT
  const auto families = CopyAvailableFamilies();
  report["family_count"] = static_cast<Json::UInt64>(families.size());

  Json::Value key_families(Json::objectValue);
  const std::set<std::string> family_set(families.begin(), families.end());
  for (const auto& family :
       std::array<const char*, 3>{"Helvetica", "Times", "Courier"}) {
    key_families[family] = family_set.find(family) != family_set.end();
  }
  report["key_families"] = std::move(key_families);

  Json::Value css_aliases(Json::objectValue);
  const std::array<std::pair<const char*, const char*>, 3> aliases = {
      {{"sans-serif", "Helvetica"},
       {"serif", "Times"},
       {"monospace", "Courier"}}};
  for (const auto& alias : aliases) {
    Json::Value item(Json::objectValue);
    item["mapped_family"] = alias.second;
    item["visible_family"] = family_set.find(alias.second) != family_set.end();
    item["descriptor_match"] = CanResolveFamilyDescriptor(alias.second);
    css_aliases[alias.first] = std::move(item);
  }
  report["css_alias_families"] = std::move(css_aliases);

  Json::Value sample(Json::arrayValue);
  const size_t sample_count = std::min<size_t>(families.size(), 16);
  for (size_t i = 0; i < sample_count; ++i) {
    sample.append(families[i]);
  }
  report["sample_families"] = std::move(sample);
#endif

  return report;
}

Json::Value BuildCoreTextFontList(const CoreTextEnvRequest& request) {
  Json::Value report = BuildBaseReport(request, "font_list_fonts");
  if (!report["backend_available"].asBool()) {
    return report;
  }

#if SKITY_FONT_HARNESS_HAS_CORETEXT
  const auto families = CopyAvailableFamilies();
  report["family_count"] = static_cast<Json::UInt64>(families.size());
  Json::Value family_list(Json::arrayValue);
  for (const auto& family_name : families) {
    Json::Value family(Json::objectValue);
    family["family_name"] = family_name;
    Json::Value styles = CopyStylesForFamily(family_name);
    family["style_count"] = static_cast<Json::UInt64>(styles.size());
    family["styles"] = std::move(styles);
    family_list.append(std::move(family));
  }
  report["families"] = std::move(family_list);
#endif

  return report;
}

}  // namespace font_harness
}  // namespace skity
