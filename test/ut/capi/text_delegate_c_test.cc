// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Unit tests for the TypefaceDelegate surface of the C API (module/capi):
// multi-font fallback when building text blobs. Cases needing real font
// files are guarded on SKITY_FONT_DIR (mirroring text/typeface_test.cc) and
// skip when the fixture is unavailable.

#include <skity_c/skity_paint.h>
#include <skity_c/skity_text.h>

#include <gtest/gtest.h>

namespace {

struct TypefaceGuard {
  skity_typeface t = nullptr;
  ~TypefaceGuard() { skity_typeface_destroy(t); }
};

struct PaintGuard {
  skity_paint p = skity_paint_create();
  ~PaintGuard() { skity_paint_destroy(p); }
};

struct DelegateGuard {
  skity_typeface_delegate d = nullptr;
  ~DelegateGuard() { skity_typeface_delegate_destroy(d); }
};

struct BlobGuard {
  skity_text_blob b = nullptr;
  ~BlobGuard() { skity_text_blob_destroy(b); }
};

// Callback state shared between the test body and the C callbacks.
struct FallbackState {
  skity_typeface result = nullptr;  // returned to skity on every probe
  int calls = 0;
  int releases = 0;
};

skity_typeface CountingFallback(void* userdata, uint32_t code_point) {
  auto* state = static_cast<FallbackState*>(userdata);
  state->calls++;
  // Only claim the pile-of-poo; anything else has no fallback.
  return code_point == 0x1F4A9 ? state->result : nullptr;
}

void CountingRelease(void* userdata) {
  static_cast<FallbackState*>(userdata)->releases++;
}

}  // namespace

TEST(CapiTextDelegate, CreateSimpleRejectsEmptyList) {
  EXPECT_EQ(skity_typeface_delegate_create_simple(nullptr, 0), nullptr);
  TypefaceGuard tf{skity_typeface_get_default()};
  ASSERT_NE(tf.t, nullptr);
  EXPECT_EQ(skity_typeface_delegate_create_simple(nullptr, 1), nullptr);
  EXPECT_EQ(skity_typeface_delegate_create_simple(&tf.t, 0), nullptr);
}

TEST(CapiTextDelegate, CreateFallbackRejectsNullCallback) {
  EXPECT_EQ(skity_typeface_delegate_create_fallback(nullptr, nullptr, nullptr),
            nullptr);
}

TEST(CapiTextDelegate, BlobRequiresPaintTypeface) {
  PaintGuard paint;
  BlobGuard blob{skity_text_blob_create("hi", paint.p)};
  EXPECT_EQ(blob.b, nullptr);

  // A missing typeface fails the delegate path too (NULL delegate or not).
  BlobGuard with_delegate{
      skity_text_blob_create_with_delegate("hi", paint.p, nullptr)};
  EXPECT_EQ(with_delegate.b, nullptr);
}

TEST(CapiTextDelegate, NullDelegateBehavesLikePlainCreate) {
  TypefaceGuard tf{skity_typeface_get_default()};
  ASSERT_NE(tf.t, nullptr);
  PaintGuard paint;
  skity_paint_set_typeface(paint.p, tf.t);

  BlobGuard plain{skity_text_blob_create("abc", paint.p)};
  BlobGuard with_null{skity_text_blob_create_with_delegate("abc", paint.p, nullptr)};
  ASSERT_NE(plain.b, nullptr);
  ASSERT_NE(with_null.b, nullptr);
  skity_rect plain_bounds = {}, null_bounds = {};
  skity_text_blob_get_bounds(plain.b, &plain_bounds);
  skity_text_blob_get_bounds(with_null.b, &null_bounds);
  EXPECT_FLOAT_EQ(plain_bounds.right - plain_bounds.left,
                  null_bounds.right - null_bounds.left);
}

TEST(CapiTextDelegate, FallbackCallbackDrivesLayoutAndRelease) {
  TypefaceGuard tf{skity_typeface_get_default()};
  ASSERT_NE(tf.t, nullptr);

  FallbackState state;
  state.result = tf.t;
  {
    DelegateGuard delegate{skity_typeface_delegate_create_fallback(
        &CountingFallback, &state, &CountingRelease)};
    ASSERT_NE(delegate.d, nullptr);

    PaintGuard paint;
    skity_paint_set_typeface(paint.p, tf.t);
    BlobGuard blob{skity_text_blob_create_with_delegate(
        "a\xF0\x9F\x92\xA9"  // U+1F4A9 PILE OF POO
        "b",
        paint.p, delegate.d)};
    // The default typeface has no glyph for the emoji, so the callback ran.
    EXPECT_GT(state.calls, 0);
    if (blob.b != nullptr) {
      skity_rect bounds = {};
      skity_text_blob_get_bounds(blob.b, &bounds);
      EXPECT_GT(bounds.right, bounds.left);
    }
  }
  // The release callback fires exactly once when the delegate is destroyed.
  EXPECT_EQ(state.releases, 1);
}

#ifdef SKITY_FONT_DIR

namespace {
constexpr const char* kRobotoRegular =
    SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf";
constexpr const char* kNotoColorEmoji =
    SKITY_FONT_DIR "fonts/resources/NotoColorEmoji.ttf";
}  // namespace

// Roboto covers the ASCII part of the text and the emoji font takes the
// pile-of-poo, exercising the multi-font fallback path end to end.
TEST(CapiTextDelegate, SimpleDelegateFallsBackAcrossFonts) {
  TypefaceGuard roboto{skity_typeface_make_from_file(kRobotoRegular)};
  TypefaceGuard emoji{skity_typeface_make_from_file(kNotoColorEmoji)};
  ASSERT_NE(roboto.t, nullptr);
#if defined(__APPLE__)
  // NotoColorEmoji is a CBDT/CBLC bitmap font; the CoreText port may reject
  // it, in which case the fallback list is unusable and there is nothing to
  // assert beyond graceful failure.
  if (emoji.t == nullptr) {
    GTEST_SKIP() << "CoreText rejected NotoColorEmoji.ttf";
  }
#endif
  if (emoji.t == nullptr) {
    GTEST_SKIP() << "emoji font fixture unavailable";
  }

  skity_typeface faces[1] = {emoji.t};
  DelegateGuard delegate{skity_typeface_delegate_create_simple(faces, 1)};
  ASSERT_NE(delegate.d, nullptr);

  PaintGuard paint;
  skity_paint_set_typeface(paint.p, roboto.t);
  BlobGuard blob{skity_text_blob_create_with_delegate(
      "ok \xF0\x9F\x92\xA9"  // U+1F4A9 PILE OF POO
      "!",
      paint.p, delegate.d)};
  ASSERT_NE(blob.b, nullptr);
  skity_rect bounds = {};
  skity_text_blob_get_bounds(blob.b, &bounds);
  EXPECT_GT(bounds.right, bounds.left);
  EXPECT_GT(bounds.bottom, bounds.top);
}

#endif  // SKITY_FONT_DIR
