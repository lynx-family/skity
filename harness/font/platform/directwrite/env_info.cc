// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/platform/directwrite/env_info.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
// clang-format off
#include "src/text/ports/win/dwrite_version.hpp"
#include "src/base/platform/win/lean_windows.hpp"
#include "src/text/ports/win/scoped_com_ptr.hpp"

#include <dwrite.h>
#include <dwrite_2.h>
// clang-format on
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

#ifndef SKITY_FONT_HARNESS_HAS_DIRECTWRITE
#define SKITY_FONT_HARNESS_HAS_DIRECTWRITE 0
#endif

#ifndef SKITY_FONT_HARNESS_TARGET_PLATFORM
#define SKITY_FONT_HARNESS_TARGET_PLATFORM "unknown"
#endif

namespace skity {
namespace font_harness {

namespace {

constexpr const char* kBackend = "directwrite";
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
  if (target_platform.rfind("windows-", 0) == 0) {
    return "Windows";
  }

  const std::string system_name = SKITY_FONT_HARNESS_CMAKE_SYSTEM_NAME;
  if (system_name == "Windows") {
    return "Windows";
  }
  return system_name.empty() ? "unknown" : system_name;
}

std::string FormatHex(uint32_t value) {
  std::ostringstream stream;
  stream << "0x" << std::uppercase << std::hex << std::setw(8)
         << std::setfill('0') << value;
  return stream.str();
}

std::string DetectOSVersion() {
#if defined(_WIN32)
  using RtlGetVersionProc = LONG(WINAPI*)(OSVERSIONINFOW*);
  HMODULE module = GetModuleHandleW(L"ntdll.dll");
  if (module != nullptr) {
    auto proc = reinterpret_cast<RtlGetVersionProc>(
        GetProcAddress(module, "RtlGetVersion"));
    if (proc != nullptr) {
      OSVERSIONINFOW info = {};
      info.dwOSVersionInfoSize = sizeof(info);
      if (proc(&info) == 0) {
        std::ostringstream stream;
        stream << info.dwMajorVersion << "." << info.dwMinorVersion << "."
               << info.dwBuildNumber;
        return stream.str();
      }
    }
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
  options["SKITY_FONT_HARNESS_HAS_DIRECTWRITE"] =
      static_cast<bool>(SKITY_FONT_HARNESS_HAS_DIRECTWRITE);
  return options;
}

Json::Value BuildBaseReport(const DirectWriteEnvRequest& request,
                            const std::string& artifact_type) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = artifact_type;
  report["platform"] = DetectPlatformName();
  report["platform_id"] = std::string(kTargetPlatform) == "windows-directwrite"
                              ? "windows-dwrite"
                              : kTargetPlatform;
  report["target_platform"] = kTargetPlatform;
  report["backend"] = kBackend;
  report["os_version"] = DetectOSVersion();
  const std::string commit = ResolveSkityCommit(request.repo_root);
  report["skity_commit"] = commit.empty() ? "unknown" : commit;
  report["cmake_options"] = BuildCMakeOptions();
  report["backend_available"] =
      static_cast<bool>(SKITY_FONT_HARNESS_HAS_DIRECTWRITE);
  if (!static_cast<bool>(SKITY_FONT_HARNESS_HAS_DIRECTWRITE)) {
    report["reason_code"] = "backend_unavailable";
    report["message"] = "DirectWrite backend is unavailable; build on Windows";
  }
  return report;
}

#if defined(_WIN32) && SKITY_FONT_HARNESS_HAS_DIRECTWRITE

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return {};
  }
  int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0,
                                 nullptr, nullptr);
  if (size <= 1) {
    return {};
  }
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size,
                      nullptr, nullptr);
  result.resize(static_cast<size_t>(size - 1));
  return Trim(std::move(result));
}

std::wstring DetectUserLocaleName() {
  WCHAR locale_name[LOCALE_NAME_MAX_LENGTH] = {};
  if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
    return locale_name;
  }
  return L"";
}

std::wstring DetectSystemLocaleName() {
  WCHAR locale_name[LOCALE_NAME_MAX_LENGTH] = {};
  if (GetSystemDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
    return locale_name;
  }
  return L"";
}

Json::Value BuildStringArray(const std::vector<std::string>& values) {
  Json::Value array(Json::arrayValue);
  for (const auto& value : values) {
    array.append(value);
  }
  return array;
}

std::vector<std::string> ParseMultiStringLanguages(const std::wstring& value) {
  std::vector<std::string> languages;
  const wchar_t* cursor = value.c_str();
  while (*cursor != L'\0') {
    std::wstring language(cursor);
    std::string utf8 = WideToUtf8(language);
    if (!utf8.empty()) {
      languages.push_back(std::move(utf8));
    }
    cursor += language.size() + 1;
  }
  return languages;
}

using GetPreferredUILanguagesProc = BOOL(WINAPI*)(DWORD, PULONG, PZZWSTR,
                                                  PULONG);

std::vector<std::string> DetectPreferredUILanguages(
    GetPreferredUILanguagesProc proc) {
  ULONG count = 0;
  ULONG buffer_size = 0;
  if (!proc(MUI_LANGUAGE_NAME, &count, nullptr, &buffer_size) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return {};
  }
  if (buffer_size == 0) {
    return {};
  }

  std::wstring buffer(static_cast<size_t>(buffer_size), L'\0');
  if (!proc(MUI_LANGUAGE_NAME, &count, buffer.data(), &buffer_size)) {
    return {};
  }
  return ParseMultiStringLanguages(buffer);
}

BOOL CALLBACK EnumInstalledUILanguage(WCHAR* language, LONG_PTR context) {
  auto* languages = reinterpret_cast<std::vector<std::string>*>(context);
  std::string utf8 = WideToUtf8(language);
  if (!utf8.empty()) {
    languages->push_back(std::move(utf8));
  }
  return TRUE;
}

std::vector<std::string> DetectInstalledUILanguages() {
  std::vector<std::string> languages;
  EnumUILanguagesW(EnumInstalledUILanguage, MUI_LANGUAGE_NAME,
                   reinterpret_cast<LONG_PTR>(&languages));
  std::sort(languages.begin(), languages.end());
  languages.erase(std::unique(languages.begin(), languages.end()),
                  languages.end());
  return languages;
}

Json::Value BuildWindowsLocaleInfo(const std::wstring& user_locale) {
  Json::Value info(Json::objectValue);
  info["user_default_locale"] = WideToUtf8(user_locale);
  info["system_default_locale"] = WideToUtf8(DetectSystemLocaleName());
  info["user_preferred_ui_languages"] =
      BuildStringArray(DetectPreferredUILanguages(GetUserPreferredUILanguages));
  info["system_preferred_ui_languages"] = BuildStringArray(
      DetectPreferredUILanguages(GetSystemPreferredUILanguages));
  info["installed_ui_languages"] =
      BuildStringArray(DetectInstalledUILanguages());
  return info;
}

struct DirectWriteFactoryResult {
  ScopedComPtr<IDWriteFactory> factory;
  std::string library;
  std::string proc_name;
  HRESULT hr = E_FAIL;
};

HRESULT LoadDWriteProc(HMODULE* module, const wchar_t* library, DWORD flags,
                       const char* proc_name, FARPROC* proc) {
  *module = LoadLibraryExW(library, nullptr, flags);
  if (*module == nullptr) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    return IS_ERROR(hr) ? hr : HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
  }

  *proc = GetProcAddress(*module, proc_name);
  if (*proc == nullptr) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    return IS_ERROR(hr) ? hr : HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  }
  return S_OK;
}

DirectWriteFactoryResult CreateDirectWriteFactory() {
  using DWriteCreateFactoryProc =
      HRESULT(WINAPI*)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**);

  DirectWriteFactoryResult result;
  HMODULE module = nullptr;
  FARPROC proc = nullptr;
  result.hr = LoadDWriteProc(&module, L"DWriteCore.dll",
                             LOAD_LIBRARY_SEARCH_DEFAULT_DIRS,
                             "DWriteCoreCreateFactory", &proc);
  if (SUCCEEDED(result.hr)) {
    result.library = "DWriteCore.dll";
    result.proc_name = "DWriteCoreCreateFactory";
  } else {
    result.hr =
        LoadDWriteProc(&module, L"dwrite.dll", LOAD_LIBRARY_SEARCH_SYSTEM32,
                       "DWriteCreateFactory", &proc);
    if (SUCCEEDED(result.hr)) {
      result.library = "dwrite.dll";
      result.proc_name = "DWriteCreateFactory";
    }
  }

  if (FAILED(result.hr)) {
    return result;
  }

  auto create_factory = reinterpret_cast<DWriteCreateFactoryProc>(proc);
  IDWriteFactory* raw_factory = nullptr;
  result.hr =
      create_factory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                     reinterpret_cast<IUnknown**>(&raw_factory));
  if (SUCCEEDED(result.hr)) {
    result.factory.reset(raw_factory);
  }
  return result;
}

std::string CopyLocalizedString(IDWriteLocalizedStrings* strings,
                                const std::wstring& preferred_locale) {
  if (strings == nullptr || strings->GetCount() == 0) {
    return {};
  }

  UINT32 index = 0;
  BOOL exists = FALSE;
  if (!preferred_locale.empty()) {
    strings->FindLocaleName(preferred_locale.c_str(), &index, &exists);
  }
  if (!exists) {
    strings->FindLocaleName(L"en-us", &index, &exists);
  }
  if (!exists) {
    index = 0;
  }

  UINT32 length = 0;
  if (FAILED(strings->GetStringLength(index, &length))) {
    return {};
  }
  std::wstring value(static_cast<size_t>(length + 1), L'\0');
  if (FAILED(strings->GetString(index, value.data(), length + 1))) {
    return {};
  }
  value.resize(length);
  return WideToUtf8(value);
}

std::string CopyFamilyName(IDWriteFontFamily* family,
                           const std::wstring& preferred_locale) {
  ScopedComPtr<IDWriteLocalizedStrings> names;
  if (FAILED(family->GetFamilyNames(&names))) {
    return {};
  }
  return CopyLocalizedString(names.get(), preferred_locale);
}

std::string CopyFaceName(IDWriteFont* font,
                         const std::wstring& preferred_locale) {
  ScopedComPtr<IDWriteLocalizedStrings> names;
  if (FAILED(font->GetFaceNames(&names))) {
    return {};
  }
  return CopyLocalizedString(names.get(), preferred_locale);
}

std::string CopyInformationalString(IDWriteFont* font,
                                    DWRITE_INFORMATIONAL_STRING_ID string_id,
                                    const std::wstring& preferred_locale) {
  ScopedComPtr<IDWriteLocalizedStrings> strings;
  BOOL exists = FALSE;
  if (FAILED(font->GetInformationalStrings(string_id, &strings, &exists)) ||
      !exists) {
    return {};
  }
  return CopyLocalizedString(strings.get(), preferred_locale);
}

std::string SlantToString(DWRITE_FONT_STYLE style) {
  switch (style) {
    case DWRITE_FONT_STYLE_NORMAL:
      return "upright";
    case DWRITE_FONT_STYLE_OBLIQUE:
      return "oblique";
    case DWRITE_FONT_STYLE_ITALIC:
      return "italic";
    default:
      return "unknown";
  }
}

Json::Value BuildStyle(IDWriteFont* font,
                       const std::wstring& preferred_locale) {
  Json::Value style(Json::objectValue);
  style["style_name"] = CopyFaceName(font, preferred_locale);
  style["postscript_name"] = CopyInformationalString(
      font, DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, preferred_locale);
  style["full_name"] = CopyInformationalString(
      font, DWRITE_INFORMATIONAL_STRING_FULL_NAME, preferred_locale);
  style["weight"] = static_cast<int>(font->GetWeight());
  style["width"] = static_cast<int>(font->GetStretch());
  style["slant"] = SlantToString(font->GetStyle());
  style["simulations"] = static_cast<int>(font->GetSimulations());
  return style;
}

std::vector<Json::Value> CopyFamilies(IDWriteFontCollection* collection,
                                      const std::wstring& preferred_locale) {
  std::vector<Json::Value> families;
  if (collection == nullptr) {
    return families;
  }

  const UINT32 count = collection->GetFontFamilyCount();
  families.reserve(count);
  for (UINT32 family_index = 0; family_index < count; ++family_index) {
    ScopedComPtr<IDWriteFontFamily> family;
    if (FAILED(collection->GetFontFamily(family_index, &family))) {
      continue;
    }

    Json::Value family_value(Json::objectValue);
    family_value["family_name"] =
        CopyFamilyName(family.get(), preferred_locale);
    family_value["directwrite_family_index"] = family_index;
    const UINT32 style_count = family->GetFontCount();
    family_value["style_count"] = static_cast<Json::UInt64>(style_count);

    Json::Value styles(Json::arrayValue);
    std::vector<Json::Value> style_values;
    style_values.reserve(style_count);
    for (UINT32 style_index = 0; style_index < style_count; ++style_index) {
      ScopedComPtr<IDWriteFont> font;
      if (FAILED(family->GetFont(style_index, &font))) {
        continue;
      }
      Json::Value style = BuildStyle(font.get(), preferred_locale);
      style["directwrite_style_index"] = style_index;
      style_values.push_back(std::move(style));
    }

    std::sort(
        style_values.begin(), style_values.end(),
        [](const Json::Value& lhs, const Json::Value& rhs) {
          const std::string lhs_key = lhs["postscript_name"].asString().empty()
                                          ? lhs["style_name"].asString()
                                          : lhs["postscript_name"].asString();
          const std::string rhs_key = rhs["postscript_name"].asString().empty()
                                          ? rhs["style_name"].asString()
                                          : rhs["postscript_name"].asString();
          return lhs_key < rhs_key;
        });
    for (auto& style : style_values) {
      styles.append(std::move(style));
    }
    family_value["styles"] = std::move(styles);
    families.push_back(std::move(family_value));
  }

  std::sort(families.begin(), families.end(),
            [](const Json::Value& lhs, const Json::Value& rhs) {
              return lhs["family_name"].asString() <
                     rhs["family_name"].asString();
            });
  return families;
}

bool FindFamily(IDWriteFontCollection* collection,
                const std::wstring& family_name) {
  if (collection == nullptr || family_name.empty()) {
    return false;
  }
  UINT32 index = 0;
  BOOL exists = FALSE;
  if (FAILED(
          collection->FindFamilyName(family_name.c_str(), &index, &exists))) {
    return false;
  }
  return exists;
}

Json::Value BuildFactoryInfo(const DirectWriteFactoryResult& factory_result,
                             IDWriteFontCollection* collection,
                             const std::wstring& locale_name) {
  Json::Value info(Json::objectValue);
  info["factory_created"] = factory_result.factory.get() != nullptr;
  info["library"] = factory_result.library;
  info["proc_name"] = factory_result.proc_name;
  info["hresult"] = FormatHex(static_cast<uint32_t>(factory_result.hr));
  info["user_locale"] = WideToUtf8(locale_name);

  if (factory_result.factory) {
    ScopedComPtr<IDWriteFactory2> factory2;
    const bool has_factory2 =
        SUCCEEDED(factory_result.factory->QueryInterface(&factory2));
    info["has_factory2"] = has_factory2;
    if (has_factory2) {
      ScopedComPtr<IDWriteFontFallback> fallback;
      info["has_system_font_fallback"] =
          SUCCEEDED(factory2->GetSystemFontFallback(&fallback));
    } else {
      info["has_system_font_fallback"] = false;
    }
  } else {
    info["has_factory2"] = false;
    info["has_system_font_fallback"] = false;
  }

  info["system_collection_created"] = collection != nullptr;
  return info;
}

void MarkFactoryFailure(Json::Value* report,
                        const DirectWriteFactoryResult& factory_result) {
  (*report)["backend_available"] = false;
  (*report)["reason_code"] = "backend_unavailable";
  (*report)["message"] = "DirectWrite factory could not be created";
  (*report)["directwrite"]["factory_created"] = false;
  (*report)["directwrite"]["library"] = factory_result.library;
  (*report)["directwrite"]["proc_name"] = factory_result.proc_name;
  (*report)["directwrite"]["hresult"] =
      FormatHex(static_cast<uint32_t>(factory_result.hr));
}

#endif

}  // namespace

Json::Value BuildDirectWriteEnvInfo(const DirectWriteEnvRequest& request) {
  Json::Value report = BuildBaseReport(request, "font_env_info");
  if (!report["backend_available"].asBool()) {
    return report;
  }

#if defined(_WIN32) && SKITY_FONT_HARNESS_HAS_DIRECTWRITE
  DirectWriteFactoryResult factory_result = CreateDirectWriteFactory();
  if (!factory_result.factory) {
    MarkFactoryFailure(&report, factory_result);
    return report;
  }

  ScopedComPtr<IDWriteFontCollection> collection;
  HRESULT collection_hr =
      factory_result.factory->GetSystemFontCollection(&collection, FALSE);
  const std::wstring locale_name = DetectUserLocaleName();
  report["directwrite"] =
      BuildFactoryInfo(factory_result, collection.get(), locale_name);
  report["windows_locale"] = BuildWindowsLocaleInfo(locale_name);
  if (FAILED(collection_hr) || !collection) {
    report["backend_available"] = false;
    report["reason_code"] = "backend_unavailable";
    report["message"] = "DirectWrite system font collection is unavailable";
    report["directwrite"]["system_collection_hresult"] =
        FormatHex(static_cast<uint32_t>(collection_hr));
    return report;
  }

  const auto families = CopyFamilies(collection.get(), locale_name);
  report["family_count"] = static_cast<Json::UInt64>(families.size());

  Json::Value key_families(Json::objectValue);
  for (const auto& family : std::array<const wchar_t*, 4>{
           L"Segoe UI", L"Arial", L"Times New Roman", L"Courier New"}) {
    key_families[WideToUtf8(family)] = FindFamily(collection.get(), family);
  }
  report["key_families"] = std::move(key_families);

  Json::Value css_aliases(Json::objectValue);
  const std::array<std::pair<const wchar_t*, const wchar_t*>, 3> aliases = {
      {{L"sans-serif", L"Segoe UI"},
       {L"serif", L"Times New Roman"},
       {L"monospace", L"Consolas"}}};
  for (const auto& alias : aliases) {
    Json::Value item(Json::objectValue);
    item["mapped_family"] = WideToUtf8(alias.second);
    item["visible_family"] = FindFamily(collection.get(), alias.second);
    item["collection_match"] = item["visible_family"];
    css_aliases[WideToUtf8(alias.first)] = std::move(item);
  }
  report["css_alias_families"] = std::move(css_aliases);

  Json::Value sample(Json::arrayValue);
  const size_t sample_count = std::min<size_t>(families.size(), 16);
  for (size_t i = 0; i < sample_count; ++i) {
    sample.append(families[i]["family_name"].asString());
  }
  report["sample_families"] = std::move(sample);
#endif

  return report;
}

Json::Value BuildDirectWriteFontList(const DirectWriteEnvRequest& request) {
  Json::Value report = BuildBaseReport(request, "font_list_fonts");
  if (!report["backend_available"].asBool()) {
    return report;
  }

#if defined(_WIN32) && SKITY_FONT_HARNESS_HAS_DIRECTWRITE
  DirectWriteFactoryResult factory_result = CreateDirectWriteFactory();
  if (!factory_result.factory) {
    MarkFactoryFailure(&report, factory_result);
    return report;
  }

  ScopedComPtr<IDWriteFontCollection> collection;
  HRESULT collection_hr =
      factory_result.factory->GetSystemFontCollection(&collection, FALSE);
  const std::wstring locale_name = DetectUserLocaleName();
  report["directwrite"] =
      BuildFactoryInfo(factory_result, collection.get(), locale_name);
  report["windows_locale"] = BuildWindowsLocaleInfo(locale_name);
  if (FAILED(collection_hr) || !collection) {
    report["backend_available"] = false;
    report["reason_code"] = "backend_unavailable";
    report["message"] = "DirectWrite system font collection is unavailable";
    report["directwrite"]["system_collection_hresult"] =
        FormatHex(static_cast<uint32_t>(collection_hr));
    return report;
  }

  const auto families = CopyFamilies(collection.get(), locale_name);
  report["family_count"] = static_cast<Json::UInt64>(families.size());
  Json::Value family_list(Json::arrayValue);
  for (const auto& family : families) {
    family_list.append(family);
  }
  report["families"] = std::move(family_list);
#endif

  return report;
}

}  // namespace font_harness
}  // namespace skity
