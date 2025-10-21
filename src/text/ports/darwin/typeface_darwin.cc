// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/darwin/typeface_darwin.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <skity/text/ports/typeface_ct.hpp>
#include <skity/text/utf.hpp>
#include <vector>

#include "src/text/ports/darwin/scaler_context_darwin.hpp"
#include "src/text/ports/darwin/types_darwin.hpp"
#include "src/text/sfnt_header.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {

namespace {

template <typename T>
T skity_cf_retain(T t) {
  return (T)CFRetain(t);
}

std::string cf_string_to_string(UniqueCFRef<CFStringRef> str) {
  if (!str) {
    return {};
  }

  CFIndex length = CFStringGetMaximumSizeForEncoding(
                       CFStringGetLength(str.get()), kCFStringEncodingUTF8) +
                   1;

  std::vector<char> buffer(length);

  CFStringGetCString(str.get(), buffer.data(), length, kCFStringEncodingUTF8);

  return std::string(buffer.data(), length - 1);
}

SKITY_SFNT_ULONG get_font_type_tag(CTFontRef ct_font) {
  UniqueCFRef<CFNumberRef> fon_format(static_cast<CFNumberRef>(
      CTFontCopyAttribute(ct_font, kCTFontFormatAttribute)));

  if (!fon_format) {
    return 0;
  }

  SInt32 font_format_value;

  if (!CFNumberGetValue(fon_format.get(), kCFNumberSInt32Type,
                        &font_format_value)) {
    return 0;
  }

  switch (font_format_value) {
    case kCTFontFormatOpenTypePostScript:
      return kOpenTypeCFFTag;
    case kCTFontFormatOpenTypeTrueType:
      return kWindowsTrueTypeTag;
    case kCTFontFormatTrueType:
      return kMacTrueTypeTag;
    case kCTFontFormatPostScript:
      return kPostScriptTag;
    case kCTFontFormatBitmap:
      return kMacTrueTypeTag;
    case kCTFontFormatUnrecognized:
    default:
      return 0;
  }

  return 0;
}

uint16_t EndianSwap16(uint16_t value) {
  return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
}

uint32_t EndianSwap32(uint32_t value) {
  return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
         ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

uint32_t CalcTableChecksum(uint32_t* data, size_t length) {
  uint32_t sum = 0;
  uint32_t* dataEnd = data + ((length + 3) & ~3) / sizeof(uint32_t);
  for (; data < dataEnd; ++data) {
    sum += EndianSwap32(*data);
  }
  return sum;
}

}  // namespace

class TypefaceCache {
 public:
  // We need alter typeface with shared ownership so that we could remove
  // them from cache here.
  void Add(std::shared_ptr<TypefaceDarwin> typeface) {
    typeface_set_.emplace_back(std::move(typeface));
  }

  std::shared_ptr<TypefaceDarwin> Find(CTFontRef ct_font) {
    for (auto& typeface : typeface_set_) {
      if (CFEqual(ct_font, typeface->GetCTFont())) {
        return typeface;
      }
    }
    return nullptr;
  }

 private:
  std::vector<std::shared_ptr<TypefaceDarwin>> typeface_set_;
};

std::shared_ptr<TypefaceDarwin> TypefaceDarwin::Make(const FontStyle& style,
                                                     UniqueCTFontRef ct_font) {
  if (!ct_font) {
    return nullptr;
  }

  static NoDestructor<TypefaceCache> cache;
  static NoDestructor<std::mutex> cache_mutex;

  std::lock_guard<std::mutex> lock(*cache_mutex);
  auto typeface = cache->Find(ct_font.get());
  if (!typeface) {
    auto typeface_darwin = std::shared_ptr<TypefaceDarwin>(
        new TypefaceDarwin(style, std::move(ct_font)));
    if (typeface_darwin) {
      typeface = typeface_darwin;
      cache->Add(std::move(typeface_darwin));
    }
  }
  return typeface;
}

std::shared_ptr<TypefaceDarwin> TypefaceDarwin::MakeWithoutCache(
    const FontStyle& style, UniqueCTFontRef ct_font) {
  if (!ct_font) {
    return nullptr;
  }
  return std::shared_ptr<TypefaceDarwin>(
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

std::shared_ptr<Data> TypefaceDarwin::OnGetData() {
  if (!serialized_data_) {
    SerializeData();
  }

  return serialized_data_;
}

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
      std::static_pointer_cast<TypefaceDarwin>(
          const_cast<TypefaceDarwin*>(this)->shared_from_this()),
      desc);
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

std::shared_ptr<Typeface> TypefaceDarwin::OnMakeVariation(
    const FontArguments& args) const {
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

void TypefaceDarwin::OnGetFontDescriptor(FontDescriptor& desc) const {
  // get family name

  desc.family_name = cf_string_to_string(
      UniqueCFRef<CFStringRef>(CTFontCopyFamilyName(ct_font_.get())));
  desc.full_name = cf_string_to_string(
      UniqueCFRef<CFStringRef>(CTFontCopyFullName(ct_font_.get())));
  desc.post_script_name = cf_string_to_string(
      UniqueCFRef<CFStringRef>(CTFontCopyPostScriptName(ct_font_.get())));

  desc.factory_id = kFontFactoryID;

  // always return 0 for collection index
  desc.collection_index = 0;
}

CTFontRef TypefaceCT::CTFontFromTypeface(
    const std::shared_ptr<Typeface>& typeface) {
  auto* typeface_drawin = static_cast<const TypefaceDarwin*>(typeface.get());
  return typeface_drawin->GetCTFont();
}

std::shared_ptr<Typeface> TypefaceCT::TypefaceFromCTFont(CTFontRef ct_font) {
  CFRetain(ct_font);
  UniqueCFRef<CTFontDescriptorRef> desc(CTFontCopyFontDescriptor(ct_font));

  FontStyle style;

  ct_desc_to_font_style(desc.get(), &style);

  return TypefaceDarwin::Make(style, UniqueCTFontRef(ct_font));
}

void TypefaceDarwin::SerializeData() {
  auto font_type = get_font_type_tag(ct_font_.get());

  auto num_tables = CountTables();

  std::vector<FontTableTag> table_tags(num_tables);
  GetTableTags(table_tags.data());

  // CT seems to be unreliable in being able to obtain the type,
  // even if all we want is the first four bytes of the font resource.
  // Just the presence of the FontForge 'FFTM' table seems to throw it off.
  if (font_type == 0) {
    font_type = kWindowsTrueTypeTag;

    bool could_be_cff = false;

    constexpr uint32_t kCFFTag = SetFourByteTag('C', 'F', 'F', ' ');
    constexpr uint32_t kCFF2Tag = SetFourByteTag('C', 'F', 'F', '2');

    for (auto tag : table_tags) {
      if (tag == kCFFTag || tag == kCFF2Tag) {
        could_be_cff = true;
        break;
      }
    }

    if (could_be_cff) {
      font_type = kOpenTypeCFFTag;
    }
  }

  // Sometimes CoreGraphics incorrectly thinks a font is
  // kCTFontFormatPostScript. It is exceedingly unlikely that this is the case,
  // so double check
  if (font_type == kPostScriptTag) {
    bool could_be_typ1 = false;

    constexpr uint32_t kTYPE1Tag = SetFourByteTag('T', 'Y', 'P', '1');
    constexpr uint32_t kCIDTag = SetFourByteTag('C', 'I', 'D', ' ');

    for (auto tag : table_tags) {
      if (tag == kTYPE1Tag || tag == kCIDTag) {
        could_be_typ1 = true;
        break;
      }
    }

    if (!could_be_typ1) {
      font_type = kOpenTypeCFFTag;
    }
  }

  std::vector<size_t> table_sizes;

  size_t total_size =
      sizeof(SFNTHeader) + sizeof(SFNTTableDirectoryEntry) * num_tables;

  for (auto tag : table_tags) {
    auto table_size = GetTableSize(tag);
    total_size += (table_size + 3) & ~3;

    table_sizes.push_back(table_size);
  }

  serialized_data_ = Data::MakeFromMalloc(std::malloc(total_size), total_size);

  const char* data_start =
      reinterpret_cast<const char*>(serialized_data_->RawData());
  auto data_ptr = const_cast<char*>(data_start);

  uint16_t entry_selector = 0;
  uint16_t search_range = 1;

  while (search_range < num_tables >> 1) {
    entry_selector++;
    search_range <<= 1;
  }

  search_range <<= 4;

  uint16_t range_shift = (num_tables << 4) - search_range;

  // write font header
  SFNTHeader* header = reinterpret_cast<SFNTHeader*>(data_ptr);
  header->font_type = EndianSwap32(font_type);
  header->num_tables = EndianSwap16(num_tables);
  header->search_range = EndianSwap16(search_range);
  header->entry_selector = EndianSwap16(entry_selector);
  header->range_shift = EndianSwap16(range_shift);

  data_ptr += sizeof(SFNTHeader);

  // write tables
  auto entry = reinterpret_cast<SFNTTableDirectoryEntry*>(data_ptr);
  data_ptr += sizeof(SFNTTableDirectoryEntry) * num_tables;

  for (size_t i = 0; i < num_tables; i++) {
    auto table_size = table_sizes[i];

    GetTableData(table_tags[i], 0, table_size, data_ptr);

    entry->tag = EndianSwap32(table_tags[i]);
    entry->checksum = EndianSwap32(CalcTableChecksum(
        reinterpret_cast<SKITY_SFNT_ULONG*>(data_ptr), table_size));
    entry->offset = EndianSwap32(static_cast<uint32_t>(data_ptr - data_start));
    entry->length = EndianSwap32(static_cast<uint32_t>(table_size));

    data_ptr += (table_size + 3) & ~3;
    entry++;
  }
}

}  // namespace skity
