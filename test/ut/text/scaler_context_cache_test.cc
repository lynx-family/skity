// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/scaler_context_cache.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <skity/text/font_manager.hpp>

#include "concurrent_runner.h"
#include "src/text/scaler_context_desc.hpp"

namespace skity {

constexpr int kThreadCount = 8;
constexpr int kIterations = 500;

ScalerContextDesc MakeDesc(TypefaceID typeface_id, float text_size,
                           float scale_x = 1.f, float skew_x = 0.f) {
  ScalerContextDesc desc{};
  desc.typeface_id = typeface_id;
  desc.text_size = text_size;
  desc.scale_x = scale_x;
  desc.skew_x = skew_x;
  desc.transform = Matrix22{};
  desc.context_scale = 1.f;
  desc.stroke_width = 0.f;
  desc.miter_limit = Paint::kDefaultMiterLimit;
  desc.cap = Paint::kDefault_Cap;
  desc.join = Paint::kDefault_Join;
  desc.fake_bold = 0;
  return desc;
}

class ScalerContextCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto font_manager = FontManager::RefDefault();
    default_typeface_ = font_manager->GetDefaultTypeface(FontStyle());
  }

  bool HasTypeface() const { return default_typeface_ != nullptr; }

  std::shared_ptr<Typeface> default_typeface_ = {};
  std::shared_ptr<Typeface> custom_typeface_ = {};
};

TEST_F(ScalerContextCacheTest, GlobalScalerContextCacheIsStable) {
  auto* cache1 = ScalerContextCache::GlobalScalerContextCache();
  auto* cache2 = ScalerContextCache::GlobalScalerContextCache();
  ASSERT_NE(cache1, nullptr);
  ASSERT_NE(cache2, nullptr);
  EXPECT_EQ(cache1, cache2);
}

TEST_F(ScalerContextCacheTest, GlobalScalerContextCacheThreadSafe) {
  auto* baseline = ScalerContextCache::GlobalScalerContextCache();
  ASSERT_NE(baseline, nullptr);

  ConcurrentRunner runner(kThreadCount, kIterations);
  runner.Run([&](int) {
    auto* cache = ScalerContextCache::GlobalScalerContextCache();
    EXPECT_EQ(cache, baseline);
  });
}

TEST_F(ScalerContextCacheTest, FindOrCreateScalerContextCachesByDesc) {
  if (!HasTypeface()) {
    GTEST_SKIP();
  }

  ScalerContextCache cache;

  auto typeface_id = default_typeface_->TypefaceId();
  ScalerContextDesc desc = MakeDesc(typeface_id, 16.f);

  auto ctx1 = cache.FindOrCreateScalerContext(desc, default_typeface_);
  auto ctx2 = cache.FindOrCreateScalerContext(desc, default_typeface_);

  ASSERT_NE(ctx1, nullptr);
  ASSERT_NE(ctx2, nullptr);
  EXPECT_EQ(ctx1.get(), ctx2.get());

  ASSERT_NE(ctx1->GetScalerContext(), nullptr);
  EXPECT_EQ(ctx1->GetScalerContext()->GetDesc(), desc);
  EXPECT_NE(ctx1->GetScalerContext()->GetTypeface(), nullptr);
}

TEST_F(ScalerContextCacheTest,
       FindOrCreateScalerContextDifferentDescDifferent) {
  if (!HasTypeface()) {
    GTEST_SKIP();
  }

  ScalerContextCache cache;

  auto typeface_id = default_typeface_->TypefaceId();
  ScalerContextDesc desc1 = MakeDesc(typeface_id, 16.f);
  ScalerContextDesc desc2 = MakeDesc(typeface_id, 18.f);

  auto ctx1 = cache.FindOrCreateScalerContext(desc1, default_typeface_);
  auto ctx2 = cache.FindOrCreateScalerContext(desc2, default_typeface_);

  ASSERT_NE(ctx1, nullptr);
  ASSERT_NE(ctx2, nullptr);
  EXPECT_NE(ctx1.get(), ctx2.get());
}

TEST_F(ScalerContextCacheTest, PurgeByTypefaceRemovesMatchingOnly) {
  if (!HasTypeface()) {
    GTEST_SKIP();
  }

  ScalerContextCache cache;

  auto id_a = default_typeface_->TypefaceId();
  auto id_b = id_a + 1;

  ScalerContextDesc desc_a = MakeDesc(id_a, 16.f);
  ScalerContextDesc desc_b = MakeDesc(id_b, 16.f);

  auto ctx_a1 = cache.FindOrCreateScalerContext(desc_a, default_typeface_);
  auto ctx_b1 = cache.FindOrCreateScalerContext(desc_b, default_typeface_);
  ASSERT_NE(ctx_a1, nullptr);
  ASSERT_NE(ctx_b1, nullptr);

  cache.PurgeByTypeface(id_a);

  auto ctx_a2 = cache.FindOrCreateScalerContext(desc_a, default_typeface_);
  auto ctx_b2 = cache.FindOrCreateScalerContext(desc_b, default_typeface_);

  ASSERT_NE(ctx_a2, nullptr);
  ASSERT_NE(ctx_b2, nullptr);
  EXPECT_NE(ctx_a1.get(), ctx_a2.get());
  EXPECT_EQ(ctx_b1.get(), ctx_b2.get());
}

TEST_F(ScalerContextCacheTest, FindOrCreateScalerContextThreadSafeOnMiss) {
  if (!HasTypeface()) {
    GTEST_SKIP();
  }

  ScalerContextCache cache;

  auto typeface_id = default_typeface_->TypefaceId();
  ScalerContextDesc desc = MakeDesc(typeface_id, 16.f);

  std::shared_ptr<ScalerContextContainer> baseline;
  std::mutex baseline_mutex;

  ConcurrentRunner runner(kThreadCount, kIterations);
  runner.Run([&](int) {
    auto ctx = cache.FindOrCreateScalerContext(desc, default_typeface_);
    ASSERT_NE(ctx, nullptr);

    std::lock_guard<std::mutex> lock(baseline_mutex);
    if (!baseline) {
      baseline = ctx;
    } else {
      EXPECT_EQ(baseline.get(), ctx.get());
    }
  });
}

TEST_F(ScalerContextCacheTest, FindOrCreateScalerContextThreadSafeOnHit) {
  if (!HasTypeface()) {
    GTEST_SKIP();
  }

  ScalerContextCache cache;

  auto typeface_id = default_typeface_->TypefaceId();
  ScalerContextDesc desc = MakeDesc(typeface_id, 16.f);

  auto baseline = cache.FindOrCreateScalerContext(desc, default_typeface_);
  ASSERT_NE(baseline, nullptr);

  ConcurrentRunner runner(kThreadCount, kIterations);
  runner.Run([&](int) {
    auto ctx = cache.FindOrCreateScalerContext(desc, default_typeface_);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx.get(), baseline.get());
  });
}

#ifdef SKITY_MACOS
// TSAN is overly conservative when detecting race conditions, and its checks on
// free and reference counting result in false positives. Temporarily disable it
// on Linux and verify whether the same issue occurs on macOS.
TEST_F(ScalerContextCacheTest, PurgeByTypefaceThreadSafe) {
  if (!HasTypeface()) {
    GTEST_SKIP();
  }

  ScalerContextCache cache;

  auto typeface_id = default_typeface_->TypefaceId();
  ScalerContextDesc desc = MakeDesc(typeface_id, 16.f);

  std::atomic<int> purge_count{0};

  ConcurrentRunner runner(kThreadCount, kIterations);
  runner.Run([&](int i) {
    if ((i & 0x7) == 0) {
      cache.PurgeByTypeface(typeface_id);
      purge_count.fetch_add(1, std::memory_order_relaxed);
    } else {
      auto ctx = cache.FindOrCreateScalerContext(desc, default_typeface_);
      EXPECT_NE(ctx, nullptr);
    }
  });

  EXPECT_GT(purge_count.load(std::memory_order_relaxed), 0);
}
#endif

TEST_F(ScalerContextCacheTest, PurgeByTypefaceReleaseTypeface) {
  auto font_manager = FontManager::RefDefault();
  std::shared_ptr<Typeface> custom_typeface;
#ifdef SKITY_FONT_DIR
  custom_typeface = font_manager->MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/NotoSerif-Italic.ttf", 0);
#else
  GTEST_SKIP();
#endif
  if (!custom_typeface) {
    GTEST_SKIP();
  }

  ScalerContextCache cache;

  auto typeface_id = custom_typeface->TypefaceId();
  ScalerContextDesc desc = MakeDesc(typeface_id, 16.f);

  auto ctx1 = cache.FindOrCreateScalerContext(desc, custom_typeface);
  auto ctx2 = cache.FindOrCreateScalerContext(desc, custom_typeface);

  ASSERT_NE(ctx1, nullptr);
  ASSERT_NE(ctx2, nullptr);
  EXPECT_EQ(ctx1.get(), ctx2.get());
  EXPECT_TRUE(cache.FindScalerContext(desc));

  cache.PurgeByTypeface(typeface_id);

  EXPECT_FALSE(cache.FindScalerContext(desc));
}

TEST_F(ScalerContextCacheTest, PurgeByTypefaceReleaseTypefaceThreadSafe) {
  auto font_manager = FontManager::RefDefault();
  std::shared_ptr<Typeface> custom_typeface;
#ifdef SKITY_FONT_DIR
  custom_typeface = font_manager->MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/NotoSerif-Italic.ttf", 0);
#else
  GTEST_SKIP();
#endif
  if (!custom_typeface) {
    GTEST_SKIP();
  }

  ScalerContextCache* cache = ScalerContextCache::GlobalScalerContextCache();
  auto typeface_id = custom_typeface->TypefaceId();
  ScalerContextDesc desc = MakeDesc(typeface_id, 16.f);

  auto ctx = cache->FindOrCreateScalerContext(desc, custom_typeface);
  ASSERT_NE(ctx, nullptr);
  EXPECT_TRUE(cache->FindScalerContext(desc));
  ctx.reset();

  ConcurrentRunner runner(1, 1);
  runner.Run([&](int i) {
    // release typeface
    custom_typeface.reset();
    EXPECT_FALSE(cache->FindScalerContext(desc));
  });

  ConcurrentRunner runner2(1, 1);
  runner2.Run([&](int i) { EXPECT_FALSE(cache->FindScalerContext(desc)); });

  EXPECT_FALSE(cache->FindScalerContext(desc));
}

TEST_F(ScalerContextCacheTest, PurgeByTypefaceCacheFullThreadSafe) {
  auto font_manager = FontManager::RefDefault();
  std::shared_ptr<Typeface> custom_typeface;
#ifdef SKITY_FONT_DIR
  custom_typeface = font_manager->MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/NotoSerif-Italic.ttf", 0);
#else
  GTEST_SKIP();
#endif
  if (!custom_typeface) {
    GTEST_SKIP();
  }

  ScalerContextCache* cache = ScalerContextCache::GlobalScalerContextCache();
  auto typeface_id = custom_typeface->TypefaceId();

  for (int i = 0; i < 2096; i++) {
    ScalerContextDesc desc = MakeDesc(typeface_id, 16.f + i * 1.f);
    auto ctx = cache->FindOrCreateScalerContext(desc, custom_typeface);
    ASSERT_NE(ctx, nullptr);
    EXPECT_TRUE(cache->FindScalerContext(desc));
  }

  ConcurrentRunner runner(1, 1);
  runner.Run([&](int i) {
    // release typeface
    {
      ScalerContextDesc desc = MakeDesc(typeface_id, 16.f + 0 * 1.f);
      EXPECT_FALSE(cache->FindScalerContext(desc));
    }
    {
      ScalerContextDesc desc = MakeDesc(typeface_id, 16.f + 100 * 1.f);
      EXPECT_TRUE(cache->FindScalerContext(desc));
    }
    custom_typeface.reset();
    {
      ScalerContextDesc desc = MakeDesc(typeface_id, 16.f + 100 * 1.f);
      EXPECT_FALSE(cache->FindScalerContext(desc));
    }
  });

  ConcurrentRunner runner2(1, 1);
  runner2.Run([&](int i) {
    {
      ScalerContextDesc desc = MakeDesc(typeface_id, 16.f + 1000 * 1.f);
      EXPECT_FALSE(cache->FindScalerContext(desc));
    }
  });

  {
    ScalerContextDesc desc = MakeDesc(typeface_id, 16.f + 2000 * 1.f);
    EXPECT_FALSE(cache->FindScalerContext(desc));
  }
}

}  // namespace skity
