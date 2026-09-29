// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Unit tests for the C++ -> C bridge entry points declared in
// include/skity_hpp/skity_bridge.hpp: wrapping a native skity::Typeface* as an
// owning skity_typeface handle (strong reference via enable_shared_from_this).
// Unlike wrapper_hpp_test.cc, this TU includes the legacy headers under
// include/skity/ on purpose — the bridge consumes native C++ objects — and
// includes no skity_hpp RAII wrapper, so the namespace collision documented in
// skity_hpp/skity.hpp cannot occur.

#include <gtest/gtest.h>

#include <skity/text/typeface.hpp>
#include <skity_hpp/skity_bridge.hpp>

// Font fixture root per platform: SKITY_FONT_DIR (Linux / macOS FreeType and
// CoreText branches, points at <root>/test/ with a trailing slash) or
// SKITY_TEST_FONT_ROOT (Windows branch, points at test/fonts/resources).
#if defined(SKITY_FONT_DIR)
#define BRIDGE_TEST_FONT(name) SKITY_FONT_DIR "fonts/resources/" name
#elif defined(SKITY_TEST_FONT_ROOT)
#define BRIDGE_TEST_FONT(name) SKITY_TEST_FONT_ROOT "/" name
#endif

namespace {

TEST(BridgeFromNative, NullReturnsNull) {
  EXPECT_EQ(skity_canvas_from_native(nullptr), nullptr);
  EXPECT_EQ(skity_typeface_from_native(nullptr), nullptr);
}

TEST(BridgeFromNative, TypefaceOwningDefault) {
  auto native = skity::Typeface::GetDefaultTypeface();
  if (native == nullptr) {
    // Not every platform's default font manager provides a default typeface
    // (e.g. the GDI-based Windows manager returns nullptr); the bridge
    // semantics are independent of which typeface is bridged, so skip.
    GTEST_SKIP() << "no default typeface on this platform";
  }
  const uint16_t native_glyph = native->UnicharToGlyph('A');

  skity_typeface handle = skity_typeface_from_native(native.get());
  ASSERT_NE(handle, nullptr);
  EXPECT_EQ(skity_typeface_unichar_to_glyph(handle, 'A'), native_glyph);

  // Owning: destroying the handle only releases the handle's own reference,
  // so the caller's shared_ptr keeps the Typeface alive and usable afterwards.
  skity_typeface_destroy(handle);
  EXPECT_EQ(native->UnicharToGlyph('A'), native_glyph);
}

#ifdef BRIDGE_TEST_FONT
TEST(BridgeFromNative, TypefaceOwningFile) {
  auto native =
      skity::Typeface::MakeFromFile(BRIDGE_TEST_FONT("Roboto-Regular.ttf"));
  ASSERT_TRUE(native);

  skity_typeface handle = skity_typeface_from_native(native.get());
  ASSERT_NE(handle, nullptr);

  const uint16_t glyph = skity_typeface_unichar_to_glyph(handle, 'A');
  EXPECT_NE(glyph, 0);
  EXPECT_EQ(glyph, native->UnicharToGlyph('A'));

  uint32_t code_points[] = {'A', 'B'};
  uint16_t glyphs[2] = {};
  skity_typeface_unichars_to_glyphs(handle, code_points, 2, glyphs);
  EXPECT_EQ(glyphs[0], glyph);

  skity_typeface_destroy(handle);
  EXPECT_EQ(native->UnicharToGlyph('A'), glyph);
}

TEST(BridgeFromNative, TypefaceOutlivesCallerReference) {
  // The handle holds a strong reference: dropping the caller's own
  // shared_ptr must not affect the handle, and releasing the handle last
  // must destroy the Typeface instead of leaking it.
  skity_typeface handle;
  {
    auto native =
        skity::Typeface::MakeFromFile(BRIDGE_TEST_FONT("Roboto-Regular.ttf"));
    ASSERT_TRUE(native);
    handle = skity_typeface_from_native(native.get());
    ASSERT_NE(handle, nullptr);
    EXPECT_NE(skity_typeface_unichar_to_glyph(handle, 'A'), 0);
  }  // caller's shared_ptr released here

  EXPECT_NE(skity_typeface_unichar_to_glyph(handle, 'A'), 0);
  skity_typeface_destroy(handle);
}
#endif  // BRIDGE_TEST_FONT

}  // namespace
