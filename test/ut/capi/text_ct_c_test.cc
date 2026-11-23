// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Unit tests for the CoreText typeface entry points declared in
// module/capi/include/skity/c/skity_text_ct.h. With the CoreText font backend
// compiled in (SKITY_TEST_CT_FONT=1) they exercise the real bridge; without it
// the same binary covers the portable NULL stubs in text_c.cpp.

#include <CoreText/CoreText.h>
#include <gtest/gtest.h>
#include <skity/c/skity_text.h>
#include <skity/c/skity_text_ct.h>

extern "C" int skity_test_text_ct_c_smoke(void);

namespace {

using MakeFromCTFont = skity_typeface (*)(CTFontRef);

// The system UI font exists on every macOS host, unlike a named family.
CTFontRef CreateSystemFont() {
  return CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
}

TEST(CapiTextCT, PureCConsumer) { EXPECT_EQ(skity_test_text_ct_c_smoke(), 0); }

TEST(CapiTextCT, NullFontReturnsNull) {
  EXPECT_EQ(skity_typeface_make_from_ct_font(nullptr), nullptr);
  EXPECT_EQ(skity_typeface_make_from_ct_font_without_cache(nullptr), nullptr);
}

#if SKITY_TEST_CT_FONT

// Checks shared by both entry points: the handle is usable, and the typeface
// keeps the font alive after the caller drops its own reference.
void ExpectBridgedSystemFont(MakeFromCTFont make) {
  CTFontRef ct_font = CreateSystemFont();
  ASSERT_NE(ct_font, nullptr);

  skity_typeface handle = make(ct_font);
  CFRelease(ct_font);
  ASSERT_NE(handle, nullptr);

  EXPECT_GT(skity_typeface_get_units_per_em(handle), 0u);
  EXPECT_NE(skity_typeface_unichar_to_glyph(handle, 'A'), 0);
  skity_typeface_destroy(handle);
}

TEST(CapiTextCT, MakeFromCTFontBridgesSystemFont) {
  ExpectBridgedSystemFont(skity_typeface_make_from_ct_font);
}

TEST(CapiTextCT, MakeFromCTFontWithoutCacheBridgesSystemFont) {
  ExpectBridgedSystemFont(skity_typeface_make_from_ct_font_without_cache);
}

TEST(CapiTextCT, CachedEntrySharesUnderlyingTypeface) {
  CTFontRef ct_font = CreateSystemFont();
  ASSERT_NE(ct_font, nullptr);

  // One handle per call, but all of them resolve to the cached typeface.
  skity_typeface first = skity_typeface_make_from_ct_font(ct_font);
  skity_typeface second = skity_typeface_make_from_ct_font(ct_font);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first, second);
  EXPECT_EQ(skity_typeface_get_unique_id(first),
            skity_typeface_get_unique_id(second));

  CFRelease(ct_font);
  skity_typeface_destroy(first);
  skity_typeface_destroy(second);
}

TEST(CapiTextCT, UncachedEntryCreatesIndependentTypefaces) {
  CTFontRef ct_font = CreateSystemFont();
  ASSERT_NE(ct_font, nullptr);

  skity_typeface first =
      skity_typeface_make_from_ct_font_without_cache(ct_font);
  skity_typeface second =
      skity_typeface_make_from_ct_font_without_cache(ct_font);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(skity_typeface_get_unique_id(first),
            skity_typeface_get_unique_id(second));

  CFRelease(ct_font);
  skity_typeface_destroy(first);
  skity_typeface_destroy(second);
}

#else

TEST(CapiTextCT, WithoutCoreTextBackendReturnsNull) {
  // Portable stubs: the entry points link but decline any font.
  CTFontRef ct_font = CreateSystemFont();
  ASSERT_NE(ct_font, nullptr);

  EXPECT_EQ(skity_typeface_make_from_ct_font(ct_font), nullptr);
  EXPECT_EQ(skity_typeface_make_from_ct_font_without_cache(ct_font), nullptr);
  CFRelease(ct_font);
}

#endif  // SKITY_TEST_CT_FONT

}  // namespace
