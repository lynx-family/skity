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

  *value = CFStringGetCStringPtr(ref.get(), kCFStringEncodingUTF8);
  return true;
}

const char* map_css_names(const char* name) {
  static const struct {
    const char* fFrom;  // name the caller specified
    const char* fTo;    // "canonical" name we map to
  } gPairs[] = {{"sans-serif", "Helvetica"},
                {"serif", "Times"},
                {"monospace", "Courier"}};

  if (name == nullptr) {
    return nullptr;
  }
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
  int delta_weight = a.weight() - b.weight();
  int delta_width = a.width() - b.width();
  return delta_weight * delta_weight + delta_width * delta_width * 100 * 100 +
         (a.slant() != b.slant()) * 900 * 900;
}

UniqueCFRef<CFDataRef> cfdata_from_skdata(std::shared_ptr<Data> const& data) {
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

UniqueCFRef<CTFontDescriptorRef> create_descriptor(CFStringRef cf_family_name,
                                                   const FontStyle& style) {
  UniqueCFRef<CFMutableDictionaryRef> cf_attributes(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));

  UniqueCFRef<CFMutableDictionaryRef> cf_traits(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));

  if (!cf_attributes || !cf_traits) {
    return nullptr;
  }

  font_style_to_ct_trait(style, cf_traits.get());
  // CTFontTraits
  CFDictionaryAddValue(cf_attributes.get(), kCTFontTraitsAttribute,
                       cf_traits.get());

  // CTFontFamilyName
  if (cf_family_name) {
    CFDictionaryAddValue(cf_attributes.get(), kCTFontFamilyNameAttribute,
                         cf_family_name);
  }

  return UniqueCFRef<CTFontDescriptorRef>(
      CTFontDescriptorCreateWithAttributes(cf_attributes.get()));
}

UniqueCFRef<CTFontDescriptorRef> create_descriptor(const char* family_name,
                                                   const FontStyle& style) {
  UniqueCFRef<CFStringRef> cf_family_name;
  if (family_name) {
    cf_family_name.reset(
        CFStringCreateWithCString(nullptr, family_name, kCFStringEncodingUTF8));
  }
  return create_descriptor(cf_family_name.get(), style);
}

UniqueCFRef<CFSetRef> name_required() {
  CFStringRef set_values[] = {kCTFontFamilyNameAttribute};
  return UniqueCFRef<CFSetRef>(CFSetCreate(
      kCFAllocatorDefault, reinterpret_cast<const void**>(set_values),
      std::size(set_values), &kCFTypeSetCallBacks));
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

FontManagerDarwin::FontManagerDarwin()
    : cf_family_names_(CTFontManagerCopyAvailableFontFamilyNames()), count_(0) {
  if (cf_family_names_) {
    count_ = static_cast<int>(CFArrayGetCount(cf_family_names_.get()));
  }
}

std::shared_ptr<FontStyleSetDarwin> FontManagerDarwin::CreateStyleSet(
    CFStringRef cf_family_name) {
  UniqueCFRef<CFMutableDictionaryRef> cf_attr(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));

  CFDictionaryAddValue(cf_attr.get(), kCTFontFamilyNameAttribute,
                       cf_family_name);

  UniqueCFRef<CTFontDescriptorRef> desc(
      CTFontDescriptorCreateWithAttributes(cf_attr.get()));

  return std::make_shared<FontStyleSetDarwin>(std::move(desc));
}

int FontManagerDarwin::OnCountFamilies() const { return count_; }

std::string FontManagerDarwin::OnGetFamilyName(int index) const {
  if (index < 0 || index >= count_) {
    return "";
  }

  CFStringRef cf_string =
      (CFStringRef)CFArrayGetValueAtIndex(cf_family_names_.get(), index);

  CFIndex length = CFStringGetMaximumSizeForEncoding(
                       CFStringGetLength(cf_string), kCFStringEncodingUTF8) +
                   1;

  std::vector<char> buffer(length);

  Boolean ok = CFStringGetCString(cf_string, buffer.data(), length,
                                  kCFStringEncodingUTF8);
  if (!ok) {
    return std::string{};
  }
  return std::string{buffer.data()};
}

std::shared_ptr<FontStyleSet> FontManagerDarwin::OnCreateStyleSet(
    int index) const {
  if (index < 0 || index >= count_) {
    return nullptr;
  }
  CFStringRef cf_family_name =
      (CFStringRef)CFArrayGetValueAtIndex(cf_family_names_.get(), index);
  return CreateStyleSet(cf_family_name);
}

std::shared_ptr<FontStyleSet> FontManagerDarwin::OnMatchFamily(
    const char* family_name) const {
  if (!family_name) {
    return nullptr;
  }
  family_name = map_css_names(family_name);
  UniqueCFRef<CFStringRef> cf_family_name = UniqueCFRef<CFStringRef>(
      CFStringCreateWithCString(nullptr, family_name, kCFStringEncodingUTF8));
  return CreateStyleSet(cf_family_name.get());
}

std::shared_ptr<Typeface> FontManagerDarwin::OnMatchFamilyStyle(
    const char* family_name, const FontStyle& style) const {
  family_name = map_css_names(family_name);
  UniqueCFRef<CTFontDescriptorRef> req_desc =
      create_descriptor(family_name, style);
  if (!req_desc) {
    return nullptr;
  }
  if (!family_name) {
    return typeface_from_desc(req_desc.get());
  }
  UniqueCFRef<CTFontDescriptorRef> resolved_desc(
      CTFontDescriptorCreateMatchingFontDescriptor(req_desc.get(),
                                                   name_required().get()));
  if (!resolved_desc) {
    return nullptr;
  }
  return typeface_from_desc(resolved_desc.get());
}

std::shared_ptr<Typeface> FontManagerDarwin::OnMatchFamilyStyleCharacter(
    const char* family_name, const FontStyle& style, const char* bcp47[],
    int bcp47Count, Unichar character) const {
  UniqueCFRef<CTFontDescriptorRef> desc = create_descriptor(family_name, style);
  if (!desc) {
    return nullptr;
  }
  auto typeface = typeface_from_desc(desc.get());

  UniqueCFRef<CFStringRef> cf_string(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&character),
      sizeof(character), kCFStringEncodingUTF32LE, false));

  if (cf_string == nullptr) {
    return nullptr;
  }

  CFRange cf_range = CFRangeMake(0, CFStringGetLength(cf_string.get()));

  UniqueCFRef<CTFontRef> ct_font(
      CTFontCreateForString(typeface->GetCTFont(), cf_string.get(), cf_range));

  if (typeface->GetCTFont() == ct_font.get()) {
    return typeface;
  }

  return TypefaceDarwin::Make(style, std::move(ct_font));
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

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static const NoDestructor<std::shared_ptr<FontManager>> font_manager(
      [] { return std::make_shared<FontManagerDarwin>(); }());
  return *font_manager;
}

}  // namespace skity
