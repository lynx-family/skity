// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_text.h>

#include <algorithm>
#include <cstring>
#include <skity/graphic/paint.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_descriptor.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/text_run.hpp>
#include <skity/text/typeface.hpp>
#include <utility>
#include <vector>

#include "handle.hpp"

namespace {

skity::FontManager* font_manager_of(skity_font_manager handle) {
  auto* w = skity::capi::resolve<skity_font_manager_s>(
      handle, SKITY_OBJECT_TYPE_FONT_MANAGER);
  return w ? static_cast<skity::FontManager*>(w->impl.get()) : nullptr;
}

skity::FontStyleSet* font_style_set_of(skity_font_style_set handle) {
  auto* w = skity::capi::resolve<skity_font_style_set_s>(
      handle, SKITY_OBJECT_TYPE_FONT_STYLE_SET);
  return w ? static_cast<skity::FontStyleSet*>(w->impl.get()) : nullptr;
}

skity::Typeface* typeface_of(skity_typeface handle) {
  auto* w = skity::capi::resolve<skity_typeface_s>(handle,
                                                   SKITY_OBJECT_TYPE_TYPEFACE);
  return w ? static_cast<skity::Typeface*>(w->impl.get()) : nullptr;
}

skity::TextBlob* text_blob_of(skity_text_blob handle) {
  auto* w = skity::capi::resolve<skity_text_blob_s>(
      handle, SKITY_OBJECT_TYPE_TEXT_BLOB);
  return w ? static_cast<skity::TextBlob*>(w->impl.get()) : nullptr;
}

skity::Font* font_of(skity_font handle) {
  auto* w = skity::capi::resolve<skity_font_s>(handle, SKITY_OBJECT_TYPE_FONT);
  return w ? static_cast<skity::Font*>(w->impl.get()) : nullptr;
}

skity::TypefaceDelegate* typeface_delegate_of(skity_typeface_delegate handle) {
  auto* w = skity::capi::resolve<skity_typeface_delegate_s>(
      handle, SKITY_OBJECT_TYPE_TYPEFACE_DELEGATE);
  return w ? static_cast<skity::TypefaceDelegate*>(w->impl.get()) : nullptr;
}

/*
 * TypefaceDelegate adapter over a C fallback callback. BreakTextRun keeps the
 * built-in empty policy (single run, per-code-point fallback), matching
 * CreateSimpleFallbackDelegate.
 */
class CallbackDelegate : public skity::TypefaceDelegate {
 public:
  CallbackDelegate(skity_typeface_fallback_fn fallback, void* userdata,
                   void (*release)(void*))
      : fallback_(fallback), userdata_(userdata), release_(release) {}

  ~CallbackDelegate() override {
    if (release_ != nullptr) {
      release_(userdata_);
    }
  }

  std::shared_ptr<skity::Typeface> Fallback(uint32_t code_point,
                                            skity::Paint const&) override {
    if (fallback_ == nullptr) {
      return nullptr;
    }
    // The handle stays owned by the C side; only a shared reference is taken.
    return skity::capi::get_impl<skity_typeface_s, skity::Typeface>(
        fallback_(userdata_, code_point), SKITY_OBJECT_TYPE_TYPEFACE);
  }

  std::vector<std::vector<uint32_t>> BreakTextRun(const char*) override {
    return {};
  }

 private:
  skity_typeface_fallback_fn fallback_;
  void* userdata_;
  void (*release_)(void*);
};

skity::FontStyle to_font_style(skity_font_style s) {
  return skity::FontStyle(s.weight, s.width,
                          static_cast<skity::FontStyle::Slant>(s.slant));
}

}  // namespace

extern "C" {

skity_font_manager skity_font_manager_ref_default(void) {
  auto sp = skity::FontManager::RefDefault();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_font_manager_s>(
      SKITY_OBJECT_TYPE_FONT_MANAGER, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_font_manager_destroy(skity_font_manager manager) {
  skity::capi::destroy_handle<skity_font_manager_s>(
      manager, SKITY_OBJECT_TYPE_FONT_MANAGER);
}

skity_typeface skity_typeface_make_from_file(const char* path) {
  if (path == nullptr) return nullptr;
  auto sp = skity::Typeface::MakeFromFile(path);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_typeface skity_typeface_make_from_data(skity_data data) {
  auto bytes = skity::capi::get_impl<skity_data_s, skity::Data>(
      data, SKITY_OBJECT_TYPE_DATA);
  if (bytes == nullptr) return nullptr;
  auto sp = skity::Typeface::MakeFromData(bytes);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_typeface skity_typeface_get_default(void) {
  auto sp = skity::Typeface::GetDefaultTypeface();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_typeface_destroy(skity_typeface typeface) {
  skity::capi::destroy_handle<skity_typeface_s>(typeface,
                                                SKITY_OBJECT_TYPE_TYPEFACE);
}

void skity_typeface_unichars_to_glyphs(skity_typeface typeface,
                                       const uint32_t* uni, int32_t count,
                                       uint16_t* glyphs) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr || uni == nullptr || glyphs == nullptr || count <= 0) {
    return;
  }
  tf->UnicharsToGlyphs(uni, count, reinterpret_cast<skity::GlyphID*>(glyphs));
}

uint16_t skity_typeface_unichar_to_glyph(skity_typeface typeface,
                                         uint32_t unichar) {
  auto* tf = typeface_of(typeface);
  return tf ? tf->UnicharToGlyph(unichar) : 0;
}

skity_data skity_typeface_get_data(skity_typeface typeface) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr) {
    return nullptr;
  }
  auto data = tf->GetData();
  if (data == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_data_s>(
      SKITY_OBJECT_TYPE_DATA, SKITY_HANDLE_OWNING, std::move(data));
}

uint32_t skity_typeface_get_units_per_em(skity_typeface typeface) {
  auto* tf = typeface_of(typeface);
  return tf ? tf->GetUnitsPerEm() : 0u;
}

void skity_typeface_get_font_style(skity_typeface typeface,
                                   skity_font_style* out) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr || out == nullptr) {
    return;
  }
  skity::FontStyle style = tf->GetFontStyle();
  out->weight = style.weight();
  out->width = style.width();
  out->slant = static_cast<skity_font_slant>(style.slant());
}

uint32_t skity_typeface_get_unique_id(skity_typeface typeface) {
  auto* tf = typeface_of(typeface);
  return tf ? tf->TypefaceId() : 0u;
}

uint32_t skity_typeface_is_bold(skity_typeface typeface) {
  auto* tf = typeface_of(typeface);
  return tf && tf->IsBold() ? 1u : 0u;
}

uint32_t skity_typeface_is_italic(skity_typeface typeface) {
  auto* tf = typeface_of(typeface);
  return tf && tf->IsItalic() ? 1u : 0u;
}

uint32_t skity_typeface_contain_glyph(skity_typeface typeface,
                                      uint32_t unichar) {
  auto* tf = typeface_of(typeface);
  return tf && tf->ContainGlyph(unichar) ? 1u : 0u;
}

uint32_t skity_typeface_contains_color_table(skity_typeface typeface) {
  auto* tf = typeface_of(typeface);
  return tf && tf->ContainsColorTable() ? 1u : 0u;
}

int32_t skity_typeface_count_tables(skity_typeface typeface) {
  auto* tf = typeface_of(typeface);
  return tf ? tf->CountTables() : 0;
}

int32_t skity_typeface_get_table_tags(skity_typeface typeface, uint32_t* tags,
                                      int32_t count) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr) {
    return 0;
  }
  if (tags == nullptr || count <= 0) {
    return tf->CountTables();
  }
  return tf->GetTableTags(reinterpret_cast<skity::FontTableTag*>(tags));
}

size_t skity_typeface_get_table_size(skity_typeface typeface, uint32_t tag) {
  auto* tf = typeface_of(typeface);
  return tf ? tf->GetTableSize(static_cast<skity::FontTableTag>(tag)) : 0u;
}

size_t skity_typeface_get_table_data(skity_typeface typeface, uint32_t tag,
                                     size_t offset, size_t length, void* data) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr || data == nullptr || length == 0) {
    return 0;
  }
  return tf->GetTableData(static_cast<skity::FontTableTag>(tag), offset, length,
                          data);
}

int32_t skity_typeface_get_variation_position(
    skity_typeface typeface, skity_variation_coordinate* coordinates,
    int32_t count) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr) {
    return 0;
  }
  // GetVariationDesignPosition() returns by value; keep the temporary
  // alive — binding a reference directly to its member would dangle.
  auto position = tf->GetVariationDesignPosition();
  const auto& coords = position.GetCoordinates();
  if (coordinates == nullptr || count <= 0) {
    return static_cast<int32_t>(coords.size());
  }
  int32_t n = static_cast<int32_t>(
      std::min<size_t>(coords.size(), static_cast<size_t>(count)));
  for (int32_t i = 0; i < n; i++) {
    coordinates[i].axis = coords[i].axis;
    coordinates[i].value = coords[i].value;
  }
  return n;
}

int32_t skity_typeface_get_variation_axes(skity_typeface typeface,
                                          skity_variation_axis* axes,
                                          int32_t count) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr) {
    return 0;
  }
  auto params = tf->GetVariationDesignParameters();
  if (axes == nullptr || count <= 0) {
    return static_cast<int32_t>(params.size());
  }
  int32_t n = static_cast<int32_t>(
      std::min<size_t>(params.size(), static_cast<size_t>(count)));
  for (int32_t i = 0; i < n; i++) {
    axes[i].tag = params[i].tag;
    axes[i].min = params[i].min;
    axes[i].def = params[i].def;
    axes[i].max = params[i].max;
    axes[i].hidden = params[i].hidden ? 1u : 0u;
  }
  return n;
}

skity_typeface skity_typeface_make_variation(skity_typeface typeface,
                                             const skity_font_arguments* args) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr || args == nullptr) {
    return nullptr;
  }
  skity::FontArguments font_args;
  font_args.SetCollectionIndex(args->collection_index);
  if (args->variation_coordinates != nullptr &&
      args->variation_coordinate_count > 0) {
    skity::VariationPosition position;
    for (uint32_t i = 0; i < args->variation_coordinate_count; i++) {
      position.AddCoordinate(args->variation_coordinates[i].axis,
                             args->variation_coordinates[i].value);
    }
    font_args.SetVariationDesignPosition(position);
  }
  auto varied = tf->MakeVariation(font_args);
  if (varied == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(varied));
}

skity_typeface_delegate skity_typeface_delegate_create_simple(
    const skity_typeface* typefaces, uint32_t count) {
  if (typefaces == nullptr || count == 0) {
    return nullptr;
  }
  std::vector<std::shared_ptr<skity::Typeface>> faces;
  faces.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    auto tf = skity::capi::get_impl<skity_typeface_s, skity::Typeface>(
        typefaces[i], SKITY_OBJECT_TYPE_TYPEFACE);
    if (tf == nullptr) {
      return nullptr;
    }
    faces.push_back(std::move(tf));
  }
  auto delegate = skity::TypefaceDelegate::CreateSimpleFallbackDelegate(faces);
  if (delegate == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::TypefaceDelegate> impl(std::move(delegate));
  return skity::capi::alloc_handle<skity_typeface_delegate_s>(
      SKITY_OBJECT_TYPE_TYPEFACE_DELEGATE, SKITY_HANDLE_OWNING,
      std::move(impl));
}

skity_typeface_delegate skity_typeface_delegate_create_fallback(
    skity_typeface_fallback_fn fallback, void* userdata,
    void (*release)(void* userdata)) {
  if (fallback == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_delegate_s>(
      SKITY_OBJECT_TYPE_TYPEFACE_DELEGATE, SKITY_HANDLE_OWNING,
      std::make_shared<CallbackDelegate>(fallback, userdata, release));
}

void skity_typeface_delegate_destroy(skity_typeface_delegate delegate) {
  skity::capi::destroy_handle<skity_typeface_delegate_s>(
      delegate, SKITY_OBJECT_TYPE_TYPEFACE_DELEGATE);
}

skity_text_blob skity_text_blob_create_with_delegate(
    const char* text, skity_paint paint, skity_typeface_delegate delegate) {
  if (text == nullptr) {
    return nullptr;
  }
  auto* pw =
      skity::capi::resolve<skity_paint_s>(paint, SKITY_OBJECT_TYPE_PAINT);
  if (pw == nullptr) {
    return nullptr;
  }
  auto* p = static_cast<skity::Paint*>(pw->impl.get());
  skity::TextBlobBuilder builder;
  auto blob = builder.BuildTextBlob(text, *p, typeface_delegate_of(delegate));
  if (blob == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, SKITY_HANDLE_OWNING, std::move(blob));
}

skity_text_blob skity_text_blob_create(const char* text, skity_paint paint) {
  if (text == nullptr) {
    return nullptr;
  }
  auto* pw =
      skity::capi::resolve<skity_paint_s>(paint, SKITY_OBJECT_TYPE_PAINT);
  if (pw == nullptr) {
    return nullptr;
  }
  auto* p = static_cast<skity::Paint*>(pw->impl.get());
  skity::TextBlobBuilder builder;
  auto blob = builder.BuildTextBlob(text, *p);
  if (blob == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, SKITY_HANDLE_OWNING, std::move(blob));
}

skity_text_blob skity_text_blob_create_from_glyphs(skity_font font,
                                                   const uint16_t* glyph_ids,
                                                   const float* pos_x,
                                                   const float* pos_y,
                                                   size_t count) {
  auto* f = font_of(font);
  if (f == nullptr || (glyph_ids == nullptr && count > 0)) {
    return nullptr;
  }
  // pos_y without pos_x has no TextRun constructor to map onto.
  if (pos_y != nullptr && pos_x == nullptr) {
    return nullptr;
  }
  std::vector<skity::GlyphID> glyphs(glyph_ids, glyph_ids + count);
  std::vector<skity::TextRun> runs;
  if (pos_y != nullptr) {
    runs.emplace_back(*f, std::move(glyphs),
                      std::vector<float>(pos_x, pos_x + count),
                      std::vector<float>(pos_y, pos_y + count));
  } else if (pos_x != nullptr) {
    runs.emplace_back(*f, std::move(glyphs),
                      std::vector<float>(pos_x, pos_x + count));
  } else {
    runs.emplace_back(*f, std::move(glyphs));
  }
  auto blob = std::make_shared<skity::TextBlob>(std::move(runs));
  return skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, SKITY_HANDLE_OWNING, std::move(blob));
}

void skity_text_blob_destroy(skity_text_blob blob) {
  skity::capi::destroy_handle<skity_text_blob_s>(blob,
                                                 SKITY_OBJECT_TYPE_TEXT_BLOB);
}

void skity_text_blob_get_bounds(skity_text_blob blob, skity_rect* out) {
  if (out == nullptr) return;
  auto* b = text_blob_of(blob);
  if (b == nullptr) return;
  skity::Rect r = b->GetBoundsRect();
  out->left = r.Left();
  out->top = r.Top();
  out->right = r.Right();
  out->bottom = r.Bottom();
}

void skity_text_blob_compute_bounds(int32_t count, const uint16_t* glyphs,
                                    const float* pos_x, const float* pos_y,
                                    skity_font font, skity_paint paint,
                                    skity_rect* out) {
  if (out == nullptr) return;
  auto* f = font_of(font);
  auto* pw =
      skity::capi::resolve<skity_paint_s>(paint, SKITY_OBJECT_TYPE_PAINT);
  if (f == nullptr || pw == nullptr || glyphs == nullptr || pos_x == nullptr ||
      pos_y == nullptr || count <= 0) {
    return;
  }
  auto* p = static_cast<skity::Paint*>(pw->impl.get());
  skity::Rect r = skity::TextBlob::ComputeBounds(
      (uint32_t)count, reinterpret_cast<const skity::GlyphID*>(glyphs), pos_x,
      pos_y, *f, *p);
  out->left = r.Left();
  out->top = r.Top();
  out->right = r.Right();
  out->bottom = r.Bottom();
}

int32_t skity_font_manager_count_families(skity_font_manager manager) {
  auto* m = font_manager_of(manager);
  return m ? m->CountFamilies() : 0;
}

int32_t skity_font_manager_get_family_name(skity_font_manager manager,
                                           int32_t index, char* buffer,
                                           int32_t buffer_size) {
  auto* m = font_manager_of(manager);
  if (m == nullptr || index < 0) {
    return 0;
  }
  std::string name = m->GetFamilyName(index);
  int32_t len = (int32_t)name.size() + 1;
  if (buffer != nullptr && buffer_size >= len) {
    std::memcpy(buffer, name.c_str(), (size_t)len);
  }
  return len;
}

skity_font_style_set skity_font_manager_create_style_set(
    skity_font_manager manager, int32_t index) {
  auto* m = font_manager_of(manager);
  if (m == nullptr) {
    return nullptr;
  }
  auto set = m->CreateStyleSet(index);
  if (set == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_font_style_set_s>(
      SKITY_OBJECT_TYPE_FONT_STYLE_SET, SKITY_HANDLE_OWNING, std::move(set));
}

skity_font_style_set skity_font_manager_match_family(skity_font_manager manager,
                                                     const char* name) {
  auto* m = font_manager_of(manager);
  if (m == nullptr || name == nullptr) {
    return nullptr;
  }
  auto set = m->MatchFamily(name);
  if (set == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_font_style_set_s>(
      SKITY_OBJECT_TYPE_FONT_STYLE_SET, SKITY_HANDLE_OWNING, std::move(set));
}

skity_typeface skity_font_manager_match_family_style(skity_font_manager manager,
                                                     const char* name,
                                                     skity_font_style style) {
  auto* m = font_manager_of(manager);
  if (m == nullptr || name == nullptr) {
    return nullptr;
  }
  auto tf = m->MatchFamilyStyle(name, to_font_style(style));
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(tf));
}

skity_typeface skity_font_manager_match_family_style_character(
    skity_font_manager manager, const char* name, skity_font_style style,
    const char** bcp47, int32_t bcp47_count, uint32_t character) {
  auto* m = font_manager_of(manager);
  if (m == nullptr || name == nullptr) {
    return nullptr;
  }
  auto tf = m->MatchFamilyStyleCharacter(name, to_font_style(style), bcp47,
                                         bcp47_count, character);
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(tf));
}

skity_typeface skity_font_manager_make_from_file(skity_font_manager manager,
                                                 const char* path,
                                                 int32_t ttc_index) {
  auto* m = font_manager_of(manager);
  if (m == nullptr || path == nullptr) {
    return nullptr;
  }
  auto tf = m->MakeFromFile(path, ttc_index);
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(tf));
}

skity_typeface skity_font_manager_make_from_data(skity_font_manager manager,
                                                 skity_data data,
                                                 int32_t ttc_index) {
  auto* m = font_manager_of(manager);
  auto bytes = skity::capi::get_impl<skity_data_s, skity::Data>(
      data, SKITY_OBJECT_TYPE_DATA);
  if (m == nullptr || bytes == nullptr) {
    return nullptr;
  }
  auto tf = m->MakeFromData(bytes, ttc_index);
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(tf));
}

skity_typeface skity_font_manager_get_default_typeface(
    skity_font_manager manager, skity_font_style style) {
  auto* m = font_manager_of(manager);
  if (m == nullptr) {
    return nullptr;
  }
  auto tf = m->GetDefaultTypeface(to_font_style(style));
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(tf));
}

int32_t skity_font_style_set_count(skity_font_style_set set) {
  auto* s = font_style_set_of(set);
  return s ? s->Count() : 0;
}

void skity_font_style_set_get_style(skity_font_style_set set, int32_t index,
                                    skity_font_style* out_style,
                                    char* name_buffer, int32_t name_size) {
  auto* s = font_style_set_of(set);
  if (s == nullptr || index < 0) {
    return;
  }
  skity::FontStyle fs;
  std::string name;
  s->GetStyle(index, &fs, name_buffer != nullptr ? &name : nullptr);
  if (out_style != nullptr) {
    out_style->weight = fs.weight();
    out_style->width = fs.width();
    out_style->slant = static_cast<skity_font_slant>(fs.slant());
  }
  if (name_buffer != nullptr && name_size > 0) {
    std::strncpy(name_buffer, name.c_str(), (size_t)name_size);
    name_buffer[name_size - 1] = '\0';
  }
}

skity_typeface skity_font_style_set_create_typeface(skity_font_style_set set,
                                                    int32_t index) {
  auto* s = font_style_set_of(set);
  if (s == nullptr) {
    return nullptr;
  }
  auto tf = s->CreateTypeface(index);
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(tf));
}

skity_typeface skity_font_style_set_match_style(skity_font_style_set set,
                                                skity_font_style style) {
  auto* s = font_style_set_of(set);
  if (s == nullptr) {
    return nullptr;
  }
  auto tf = s->MatchStyle(to_font_style(style));
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(tf));
}

void skity_font_style_set_destroy(skity_font_style_set set) {
  skity::capi::destroy_handle<skity_font_style_set_s>(
      set, SKITY_OBJECT_TYPE_FONT_STYLE_SET);
}

int32_t skity_typeface_get_font_descriptor(skity_typeface typeface,
                                           skity_font_descriptor* out,
                                           char* family_name,
                                           int32_t family_name_size) {
  auto* tf = typeface_of(typeface);
  if (tf == nullptr) {
    return 0;
  }
  skity::FontDescriptor desc = tf->GetFontDescriptor();
  if (out != nullptr) {
    out->style.weight = desc.style.weight();
    out->style.width = desc.style.width();
    out->style.slant = static_cast<skity_font_slant>(desc.style.slant());
    out->collection_index = desc.collection_index;
    out->factory_id = desc.factory_id;
  }
  int32_t length = static_cast<int32_t>(desc.family_name.size()) + 1;
  if (family_name == nullptr || family_name_size <= 0) {
    return length;
  }
  std::strncpy(family_name, desc.family_name.c_str(),
               (size_t)family_name_size - 1);
  family_name[family_name_size - 1] = '\0';
  return length;
}

}  // extern "C"
