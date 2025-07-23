// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/darwin/typeface_darwin.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

#include <cstring>
#include <mutex>
#include <skity/text/ports/typeface_ct.hpp>
#include <skity/text/utf.hpp>
#include <vector>

#include "src/text/ports/darwin/scaler_context_darwin.hpp"
#include "src/text/ports/darwin/types_darwin.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {

template <typename T>
T skity_cf_retain(T t) {
  return (T)CFRetain(t);
}

class TypefaceCache {
 public:
  // We need alter typeface with shared ownership so that we could remove
  // them from cache here.
  void Add(std::unique_ptr<TypefaceDarwin> typeface) {
    typeface_set_.emplace_back(std::move(typeface));
  }

  TypefaceDarwin* Find(CTFontRef ct_font) {
    for (auto& typeface : typeface_set_) {
      if (CFEqual(ct_font, typeface->GetCTFont())) {
        return typeface.get();
      }
    }
    return nullptr;
  }

 private:
  std::vector<std::unique_ptr<TypefaceDarwin>> typeface_set_;
};

TypefaceDarwin* TypefaceDarwin::Make(const FontStyle& style,
                                     UniqueCTFontRef ct_font) {
  if (!ct_font) {
    return nullptr;
  }

  static NoDestructor<TypefaceCache> cache;
  static NoDestructor<std::mutex> cache_mutex;

  std::lock_guard<std::mutex> lock(*cache_mutex);
  TypefaceDarwin* typeface = cache->Find(ct_font.get());
  if (!typeface) {
    auto typeface_darwin = std::unique_ptr<TypefaceDarwin>(
        new TypefaceDarwin(style, std::move(ct_font)));
    if (typeface_darwin) {
      typeface = typeface_darwin.get();
      cache->Add(std::move(typeface_darwin));
    }
  }
  return typeface;
}

std::unique_ptr<TypefaceDarwin> TypefaceDarwin::MakeWithoutCache(
    const FontStyle& style, UniqueCTFontRef ct_font) {
  if (!ct_font) {
    return nullptr;
  }
  return std::unique_ptr<TypefaceDarwin>(
      new TypefaceDarwin(style, std::move(ct_font)));
}

TypefaceDarwin::TypefaceDarwin(const FontStyle& style, UniqueCTFontRef ct_font)
    : Typeface(style),
      ct_font_(std::move(ct_font)),
      has_color_glyphs_(CTFontGetSymbolicTraits(ct_font_.get()) &
                        kCTFontColorGlyphsTrait) {
  variation_axes_.reset(CTFontCopyVariationAxes(ct_font_.get()));
}

TypefaceDarwin::~TypefaceDarwin() = default;

CTFontRef TypefaceDarwin::GetCTFont() const { return ct_font_.get(); }

int TypefaceDarwin::OnGetTableTags(FontTableTag* tags) const {
  CFArrayRef cf_array =
      CTFontCopyAvailableTables(ct_font_.get(), kCTFontTableOptionNoOptions);

  if (cf_array == nullptr) {
    return 0;
  }

  CFIndex count = CFArrayGetCount(cf_array);

  if (tags) {
    for (CFIndex i = 0; i < count; i++) {
      uintptr_t tag =
          reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(cf_array, i));

      tags[i] = static_cast<FontTableTag>(tag);
    }
  }

  CFRelease(cf_array);

  return count;
}

size_t TypefaceDarwin::OnGetTableData(FontTableTag tag, size_t offset,
                                      size_t length, void* data) const {
  CFDataRef cf_data = CTFontCopyTable(ct_font_.get(), (CTFontTableTag)tag,
                                      kCTFontTableOptionNoOptions);

  if (cf_data == nullptr) {
    CGFontRef cg_font = CTFontCopyGraphicsFont(ct_font_.get(), nullptr);
    cf_data = CGFontCopyTableForTag(cg_font, tag);
    CGFontRelease(cg_font);
  }

  if (cf_data == nullptr) {
    return 0;
  }

  size_t data_size = CFDataGetLength(cf_data);

  if (length > data_size - offset) {
    length = data_size - offset;
  }

  if (offset >= data_size) {
    CFRelease(cf_data);
    return 0;
  }

  if (data) {
    std::memcpy(data, CFDataGetBytePtr(cf_data) + offset, length);
  }

  CFRelease(cf_data);

  return length;
}

void TypefaceDarwin::OnCharsToGlyphs(const uint32_t* chars, int count,
                                     GlyphID* glyphs) const {
  std::vector<uint16_t> utf16_data(count * 2);
  const uint32_t* utf32 = chars;
  auto utf16 = utf16_data.data();
  auto src = utf16;
  for (int32_t i = 0; i < count; i++) {
    utf16 += UTF::ConvertToUTF16(utf32[i], utf16);
  }

  int32_t src_count = utf16 - src;
  std::vector<uint16_t> ct_glyphs(std::max(src_count, count));

  if (src_count > count) {
    CTFontGetGlyphsForCharacters(ct_font_.get(), src, ct_glyphs.data(),
                                 src_count);
  } else {
    CTFontGetGlyphsForCharacters(ct_font_.get(), src, glyphs, src_count);
  }

  if (src_count > count) {
    int32_t extra = 0;
    for (int32_t i = 0; i < count; i++) {
      glyphs[i] = ct_glyphs[i + extra];
      /**
       * Given a UTF-16 code point, returns true iff it is a leading surrogate.
       * https://unicode.org/faq/utf_bom.html#utf16-2
       */
      if (((src[i + extra]) & 0xFC00) == 0xD800) {
        extra++;
      }
    }
  }
}

Data* TypefaceDarwin::OnGetData() { return nullptr; }

uint32_t TypefaceDarwin::OnGetUPEM() const {
  CGFontRef cg_font = CTFontCopyGraphicsFont(ct_font_.get(), nullptr);

  auto pem = CGFontGetUnitsPerEm(cg_font);

  CGFontRelease(cg_font);

  return pem;
}

bool TypefaceDarwin::OnContainsColorTable() const { return has_color_glyphs_; }

std::unique_ptr<ScalerContext> TypefaceDarwin::OnCreateScalerContext(
    const ScalerContextDesc* desc) const {
  return std::make_unique<ScalerContextDarwin>(
      const_cast<TypefaceDarwin*>(this), desc);
}

VariationPosition TypefaceDarwin::OnGetVariationDesignPosition() const {
  VariationPosition position;
  if (!variation_axes_) {
    return position;
  }
  CFIndex axis_count = CFArrayGetCount(variation_axes_.get());
  if (axis_count == -1) {
    return position;
  }

  UniqueCFRef<CFDictionaryRef> ctVariation(CTFontCopyVariation(ct_font_.get()));
  if (!ctVariation) {
    return position;
  }

  for (int i = 0; i < axis_count; ++i) {
    CFTypeRef axis = CFArrayGetValueAtIndex(variation_axes_.get(), i);
    if (CFGetTypeID(axis) != CFDictionaryGetTypeID()) {
      return VariationPosition();
    }
    CFDictionaryRef axis_dict = (CFDictionaryRef)axis;

    CFNumberRef tag_ref = (CFNumberRef)CFDictionaryGetValue(
        axis_dict, kCTFontVariationAxisIdentifierKey);
    int64_t tag;
    CFNumberGetValue(tag_ref, kCFNumberLongLongType, &tag);

    CFTypeRef value_ref = CFDictionaryGetValue(ctVariation.get(), tag_ref);
    if (!value_ref) {
      value_ref =
          CFDictionaryGetValue(axis_dict, kCTFontVariationAxisDefaultValueKey);
    }
    float value;
    CFNumberGetValue((CFNumberRef)value_ref, kCFNumberFloatType, &value);

    position.AddCoordinate(tag, value);
  }

  return position;
}

std::vector<VariationAxis> TypefaceDarwin::OnGetVariationDesignParameters()
    const {
  if (!variation_axes_) {
    return {};
  }
  CFIndex axis_count = CFArrayGetCount(variation_axes_.get());
  if (axis_count == -1) {
    return {};
  }

  std::vector<VariationAxis> axes;
  for (int i = 0; i < axis_count; ++i) {
    CFTypeRef axis = CFArrayGetValueAtIndex(variation_axes_.get(), i);
    if (CFGetTypeID(axis) != CFDictionaryGetTypeID()) {
      return {};
    }
    CFDictionaryRef axis_dict = (CFDictionaryRef)axis;

    int64_t tag;
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(
                         axis_dict, kCTFontVariationAxisIdentifierKey),
                     kCFNumberLongLongType, &tag);

    float min;
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(
                         axis_dict, kCTFontVariationAxisMinimumValueKey),
                     kCFNumberFloatType, &min);
    float max;
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(
                         axis_dict, kCTFontVariationAxisMaximumValueKey),
                     kCFNumberFloatType, &max);
    float def;
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(
                         axis_dict, kCTFontVariationAxisDefaultValueKey),
                     kCFNumberFloatType, &def);
    bool hidden = false;
    static CFStringRef* kCTFontVariationAxisHiddenKeyPtr =
        static_cast<CFStringRef*>(
            dlsym(RTLD_DEFAULT, "kCTFontVariationAxisHiddenKey"));
    if (kCTFontVariationAxisHiddenKeyPtr) {
      CFTypeRef hidden_ref =
          CFDictionaryGetValue(axis_dict, *kCTFontVariationAxisHiddenKeyPtr);
      if (hidden_ref && CFGetTypeID(hidden_ref) == CFBooleanGetTypeID()) {
        hidden = CFBooleanGetValue((CFBooleanRef)hidden_ref);
      } else if (hidden_ref && CFGetTypeID(hidden_ref) == CFNumberGetTypeID()) {
        int hidden_int;
        CFNumberGetValue((CFNumberRef)hidden_ref, kCFNumberIntType,
                         &hidden_int);
        hidden = !!hidden_int;
      }
    }

    axes.emplace_back(tag, min, def, max, hidden);
  }

  return axes;
}

template <typename T>
static constexpr const T& clamp(const T& x, const T& lo, const T& hi) {
  return std::max(lo, std::min(x, hi));
}

static UniqueCFRef<CFDictionaryRef> variation_from_FontArguments(
    CTFontRef ct, CFArrayRef variation_axes, const FontArguments& args) {
  if (!variation_axes) {
    return nullptr;
  }
  CFIndex axisCount = CFArrayGetCount(variation_axes);
  UniqueCFRef<CFDictionaryRef> old_variation(CTFontCopyVariation(ct));
  const VariationPosition& position = args.GetVariationDesignPosition();

  UniqueCFRef<CFMutableDictionaryRef> new_variation(CFDictionaryCreateMutable(
      kCFAllocatorDefault, axisCount, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));

  for (int i = 0; i < axisCount; ++i) {
    CFTypeRef axis = CFArrayGetValueAtIndex(variation_axes, i);
    CFDictionaryRef axis_dict = (CFDictionaryRef)axis;
    CFNumberRef tag_ref = (CFNumberRef)CFDictionaryGetValue(
        axis_dict, kCTFontVariationAxisIdentifierKey);
    int64_t tag;
    CFNumberGetValue(tag_ref, kCFNumberLongLongType, &tag);

    float min;
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(
                         axis_dict, kCTFontVariationAxisMinimumValueKey),
                     kCFNumberFloatType, &min);
    float max;
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(
                         axis_dict, kCTFontVariationAxisMaximumValueKey),
                     kCFNumberFloatType, &max);
    float def;
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(
                         axis_dict, kCTFontVariationAxisDefaultValueKey),
                     kCFNumberFloatType, &def);

    float value = def;
    if (old_variation) {
      CFTypeRef value_ref = CFDictionaryGetValue(old_variation.get(), tag_ref);
      if (value_ref) {
        CFNumberGetValue((CFNumberRef)value_ref, kCFNumberFloatType, &value);
      }
    }
    for (int j = position.GetCoordinates().size(); j-- > 0;) {
      if (position.GetCoordinates()[j].axis == tag) {
        value = clamp<float>(position.GetCoordinates()[j].value, min, max);
        break;
      }
    }
    UniqueCFRef<CFNumberRef> value_ref(
        CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &value));
    CFDictionaryAddValue(new_variation.get(), tag_ref, value_ref.get());
  }

  return std::move(new_variation);
}

Typeface* TypefaceDarwin::OnMakeVariation(const FontArguments& args) const {
  UniqueCFRef<CFDictionaryRef> variation =
      variation_from_FontArguments(ct_font_.get(), variation_axes_.get(), args);
  UniqueCFRef<CTFontRef> variant_font;
  FontStyle font_style;
  if (variation) {
    UniqueCFRef<CFMutableDictionaryRef> attributes(CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));

    CFDictionarySetValue(attributes.get(), kCTFontVariationAttribute,
                         variation.get());
    UniqueCFRef<CTFontDescriptorRef> variant_desc(
        CTFontDescriptorCreateWithAttributes(attributes.get()));
    ct_desc_to_font_style(variant_desc.get(), &font_style);
    variant_font.reset(CTFontCreateCopyWithAttributes(
        ct_font_.get(), 0, nullptr, variant_desc.get()));
  } else {
    variant_font.reset((CTFontRef)CFRetain(ct_font_.get()));
    font_style = GetFontStyle();
  }
  if (!variant_font) {
    return nullptr;
  }
  return TypefaceDarwin::Make(font_style, std::move(variant_font));
}

CTFontRef TypefaceCT::CTFontFromTypeface(const Typeface* typeface) {
  auto* typeface_drawin = static_cast<const TypefaceDarwin*>(typeface);
  return typeface_drawin->GetCTFont();
}

Typeface* TypefaceCT::TypefaceFromCTFont(CTFontRef ct_font) {
  CFRetain(ct_font);
  UniqueCFRef<CTFontDescriptorRef> desc(CTFontCopyFontDescriptor(ct_font));

  FontStyle style;

  ct_desc_to_font_style(desc.get(), &style);

  return TypefaceDarwin::Make(style, UniqueCTFontRef(ct_font));
}

}  // namespace skity
