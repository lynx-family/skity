/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/darwin/font_manager_darwin.hpp"

#include <CoreText/CoreText.h>
#include <dlfcn.h>

#include <array>
#include <cmath>
#include <limits>
#include <skity/macros.hpp>

#include "src/logging.hpp"
#include "src/text/ports/darwin/types_darwin.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {

namespace {

bool find_desc_str(CTFontDescriptorRef desc, CFStringRef name,
                   std::string* value) {
  UniqueCFRef<CFStringRef> ref(
      (CFStringRef)CTFontDescriptorCopyAttribute(desc, name));
  if (!ref) {
    return false;
  }

  *value = CFStringGetCStringPtr(name, kCFStringEncodingUTF8);
  return true;
}

static const char* map_css_names(const char* name) {
  static const struct {
    const char* fFrom;  // name the caller specified
    const char* fTo;    // "canonical" name we map to
  } gPairs[] = {{"sans-serif", "Helvetica"},
                {"serif", "Times"},
                {"monospace", "Courier"}};

  for (size_t i = 0; i < std::size(gPairs); i++) {
    if (strcmp(name, gPairs[i].fFrom) == 0) {
      return gPairs[i].fTo;
    }
  }
  return name;
}

std::shared_ptr<TypefaceDarwin> typeface_from_desc(CTFontDescriptorRef desc) {
  CTFontRef ct_font = CTFontCreateWithFontDescriptor(desc, 0, nullptr);

  FontStyle style;

  ct_desc_to_font_style(desc, &style);

  return TypefaceDarwin::Make(style, UniqueCTFontRef(ct_font));
}

int32_t compute_metric(const FontStyle& a, const FontStyle& b) {
  return std::sqrt(a.weight() - b.weight()) +
         std::sqrt((a.width() - b.width()) * 100) +
         std::sqrt((a.slant() != b.slant()) * 900);
}

static UniqueCFRef<CFDataRef> cfdata_from_skdata(
    std::shared_ptr<Data> const& data) {
  size_t const size = data->Size();

  void* addr = std::malloc(size);
  std::memcpy(addr, data->RawData(), size);

  CFAllocatorContext ctx = {
      0,        // CFIndex version
      addr,     // void* info
      nullptr,  // const void *(*retain)(const void *info);
      nullptr,  // void (*release)(const void *info);
      nullptr,  // CFStringRef (*copyDescription)(const void *info);
      nullptr,  // void * (*allocate)(CFIndex size, CFOptionFlags hint, void
                // *info);
      nullptr,  // void*(*reallocate)(void* ptr,CFIndex newsize,CFOptionFlags
                // hint,void* info);
      [](void*,
         void* info) -> void {  // void (*deallocate)(void *ptr, void *info);
        std::free(info);
      },
      nullptr,  // CFIndex (*preferredSize)(CFIndex size, CFOptionFlags hint,
                // void *info);
  };
  UniqueCFRef<CFAllocatorRef> alloc(
      CFAllocatorCreate(kCFAllocatorDefault, &ctx));
  return UniqueCFRef<CFDataRef>(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, (const UInt8*)addr, size, alloc.get()));
}

}  // namespace

FontStyleSetDarwin::FontStyleSetDarwin(UniqueCFRef<CTFontDescriptorRef> desc)
    : cf_desc(std::move(desc)),
      mached_desc_(CTFontDescriptorCreateMatchingFontDescriptors(cf_desc.get(),
                                                                 nullptr)),
      typefaces_(Count()) {}

int FontStyleSetDarwin::Count() {
  if (!mached_desc_) {
    return 0;
  }

  return CFArrayGetCount(mached_desc_.get());
}

void FontStyleSetDarwin::GetStyle(int index, FontStyle* style,
                                  std::string* name) {
  int32_t count = Count();
  if (index >= count) {
    return;
  }

  CTFontDescriptorRef desc =
      (CTFontDescriptorRef)CFArrayGetValueAtIndex(mached_desc_.get(), index);

  if (style) {
    ct_desc_to_font_style(desc, style);
  }

  if (name) {
    if (!find_desc_str(desc, kCTFontStyleNameAttribute, name)) {
      name->clear();
    }
  }
}

std::shared_ptr<Typeface> FontStyleSetDarwin::CreateTypeface(int index) {
  if (index >= static_cast<int32_t>(typefaces_.size())) {
    return nullptr;
  }

  if (typefaces_[index] == nullptr) {
    typefaces_[index] = typeface_from_desc(
        (CTFontDescriptorRef)CFArrayGetValueAtIndex(mached_desc_.get(), index));
  }

  return typefaces_[index];
}

std::shared_ptr<Typeface> FontStyleSetDarwin::MatchStyle(
    const FontStyle& pattern) {
  int best_metric = std::numeric_limits<int32_t>::max();

  int32_t index = -1;

  for (int i = 0; i < Count(); ++i) {
    CTFontDescriptorRef desc =
        (CTFontDescriptorRef)CFArrayGetValueAtIndex(mached_desc_.get(), i);

    FontStyle style;
    ct_desc_to_font_style(desc, &style);

    int metric = compute_metric(pattern, style);

    if (metric < best_metric) {
      best_metric = metric;
      index = i;
    }
  }

  if (index < 0) {
    return nullptr;
  }

  return CreateTypeface(index);
}

CTFontDescriptorRef FontStyleSetDarwin::GetCTFontDescriptor() const {
  return cf_desc.get();
}

FontManagerDarwin::FontManagerDarwin() { InitSystemFamily(); }

void FontManagerDarwin::InitSystemFamily() {
  cf_family_names_.reset(CTFontManagerCopyAvailableFontFamilyNames());

  auto count = CFArrayGetCount(cf_family_names_.get());

  for (uint32_t i = 0; i < count; i++) {
    CFStringRef cf_string =
        (CFStringRef)CFArrayGetValueAtIndex(cf_family_names_.get(), i);

    CFIndex length = CFStringGetMaximumSizeForEncoding(
                         CFStringGetLength(cf_string), kCFStringEncodingUTF8) +
                     1;

    std::vector<char> buffer(length);

    CFStringGetCString(cf_string, buffer.data(), length, kCFStringEncodingUTF8);

    if (strcmp("Helvetica", buffer.data()) == 0) {
      default_name_index_ = i;
    }

    sys_family_names_.emplace_back(std::string(buffer.data()));
  }
  sys_family_names_.emplace_back("sans-serif");
  sys_family_names_.emplace_back("serif");
  sys_family_names_.emplace_back("monospace");

  sys_style_sets_.resize(sys_family_names_.size());
  DEBUG_CHECK(sys_family_names_.size() == count + 3);
  DEBUG_CHECK(sys_style_sets_.size() == count + 3);
}

int FontManagerDarwin::OnCountFamilies() const {
  return static_cast<int>(sys_family_names_.size());
}

std::string FontManagerDarwin::OnGetFamilyName(int index) const {
  if (index >= static_cast<int32_t>(sys_family_names_.size())) {
    return "";
  }

  return sys_family_names_[index];
}

std::shared_ptr<FontStyleSet> FontManagerDarwin::OnCreateStyleSet(
    int index) const {
  return nullptr;
}

std::shared_ptr<FontStyleSet> FontManagerDarwin::OnMatchFamily(
    const char* family_name) const {
  auto index = GetIndexByFamilyName(family_name);

  if (index < 0) {
    return nullptr;
  }

  return MatchFamilyByIndex(index);
}

std::shared_ptr<Typeface> FontManagerDarwin::OnMatchFamilyStyle(
    const char* family_name, const FontStyle& style) const {
  auto index = GetIndexByFamilyName(family_name);

  auto style_set = MatchFamilyByIndex(index);

  if (style_set == nullptr) {
    return nullptr;
  }

  return style_set->MatchStyle(style);
}

std::shared_ptr<Typeface> FontManagerDarwin::OnMatchFamilyStyleCharacter(
    const char* family_name, const FontStyle& style, const char* bcp47[],
    int bcp47Count, Unichar character) const {
  auto index = GetIndexByFamilyName(family_name);

  auto style_set = MatchFamilyByIndex(index);

  if (style_set == nullptr) {
    return nullptr;
  }

  auto typeface =
      std::static_pointer_cast<TypefaceDarwin>(style_set->MatchStyle(style));

  UniqueCFRef<CFStringRef> cf_string(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&character),
      sizeof(character), kCFStringEncodingUTF32LE, false));

  if (cf_string == nullptr) {
    return nullptr;
  }

  CFRange cf_range = CFRangeMake(0, CFStringGetLength(cf_string.get()));

  UniqueCFRef<CTFontRef> cf_font(
      CTFontCreateForString(typeface->GetCTFont(), cf_string.get(), cf_range));

  if (typeface->GetCTFont() == cf_font.get()) {
    return typeface;
  }

  // fallbacked by system
  return SavedFallbackTypeface(std::move(cf_font), style);
}

std::shared_ptr<Typeface> FontManagerDarwin::OnMakeFromData(
    std::shared_ptr<Data> const& data, int ttcIndex) const {
  if (ttcIndex != 0) {
    return nullptr;
  }

  if (data == nullptr || data->Size() == 0) {
    return nullptr;
  }

  UniqueCFRef<CFDataRef> cf_data(cfdata_from_skdata(data));

  UniqueCFRef<CTFontDescriptorRef> desc(
      CTFontManagerCreateFontDescriptorFromData(cf_data.get()));

  FontStyle style;

  ct_desc_to_font_style(desc.get(), &style);

  return TypefaceDarwin::MakeWithoutCache(
      style,
      UniqueCTFontRef(CTFontCreateWithFontDescriptor(desc.get(), 0, nullptr)));
}

std::shared_ptr<Typeface> FontManagerDarwin::OnMakeFromFile(
    const char path[], int ttcIndex) const {
  return OnMakeFromData(Data::MakeFromFileName(path), ttcIndex);
}

std::shared_ptr<Typeface> FontManagerDarwin::OnGetDefaultTypeface(
    FontStyle const& font_style) const {
  if (default_typeface_) {
    return default_typeface_;
  }

  return OnMatchFamilyStyle("Helvetica", font_style);
}

int32_t FontManagerDarwin::GetIndexByFamilyName(const char* family_name) const {
  if (family_name == NULL) {
    return default_name_index_;
  }

  for (int32_t i = 0; i < static_cast<int32_t>(sys_family_names_.size()); i++) {
    if (sys_family_names_[i] == family_name) {
      return i;
    }
  }

  return -1;
}

std::shared_ptr<FontStyleSetDarwin> FontManagerDarwin::MatchFamilyByIndex(
    int32_t index) const {
  if (index >= static_cast<int32_t>(sys_style_sets_.size()) || index < 0) {
    return nullptr;
  }

  if (sys_style_sets_[index] == nullptr) {
    UniqueCFRef<CFMutableDictionaryRef> cf_attr(CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));

    if (index < CFArrayGetCount(cf_family_names_.get())) {
      CFStringRef cf_family =
          (CFStringRef)CFArrayGetValueAtIndex(cf_family_names_.get(), index);
      CFDictionaryAddValue(cf_attr.get(), kCTFontFamilyNameAttribute,
                           cf_family);
    } else {
      // css names
      auto* family_name = map_css_names(sys_family_names_[index].c_str());
      CFStringRef css_cf_nmame = CFStringCreateWithCString(
          kCFAllocatorDefault, family_name, kCFStringEncodingUTF8);
      CFDictionaryAddValue(cf_attr.get(), kCTFontFamilyNameAttribute,
                           css_cf_nmame);
      CFRelease(css_cf_nmame);
    }

    UniqueCFRef<CTFontDescriptorRef> desc(
        CTFontDescriptorCreateWithAttributes(cf_attr.get()));

    sys_style_sets_[index] =
        std::make_shared<FontStyleSetDarwin>(std::move(desc));
  }

  return sys_style_sets_[index];
}

std::shared_ptr<TypefaceDarwin> FontManagerDarwin::SavedFallbackTypeface(
    UniqueCFRef<CTFontRef> ct_font, FontStyle const& style) const {
  for (auto const& tf : sys_fallbacked_) {
    if (CFEqual(tf->GetCTFont(), ct_font.get())) {
      return tf;
    }
  }

  sys_fallbacked_.emplace_back(TypefaceDarwin::Make(style, std::move(ct_font)));

  return sys_fallbacked_.back();
}

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static const NoDestructor<std::shared_ptr<FontManager>> font_manager(
      [] { return std::make_shared<FontManagerDarwin>(); }());
  return *font_manager;
}

}  // namespace skity
