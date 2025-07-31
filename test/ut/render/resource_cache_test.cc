// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>

#include "src/render/hw/hw_resource_cache.hpp"

namespace skity {
namespace testing {

struct TestResourceKey {
  int hint_value;
};

class TestResource : public HWResource<TestResourceKey, int> {
 public:
  explicit TestResource(const TestResourceKey& key) : key_(key) {}
  const TestResourceKey& GetKey() const override { return key_; }
  int GetValue() const override { return key_.hint_value; }
  size_t GetBytes() const override { return key_.hint_value * 4; }

 private:
  TestResourceKey key_;
};

struct TestResourceKeyCompare {
  bool operator()(const TestResourceKey& lhs,
                  const TestResourceKey& rhs) const {
    return lhs.hint_value < rhs.hint_value;
  }
};

struct TestResourceAllocator
    : public HWResourceAllocator<TestResourceKey, int> {
  virtual std::shared_ptr<HWResource<TestResourceKey, int>> AllocateResource(
      const TestResourceKey& key) override {
    return std::make_shared<TestResource>(key);
  }
};

using TestResourceCache =
    HWResourceCache<TestResourceKey, int, TestResourceKeyCompare>;
}  // namespace testing
}  // namespace skity

TEST(ResourceCache, ObtainAndStore) {
  skity::testing::TestResourceCache cache(
      std::make_unique<skity::testing::TestResourceAllocator>(), 1000);

  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
  skity::testing::TestResourceKey key{100};
  auto resource = cache.ObtainResource(key);
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
  cache.StoreResource(resource);
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(400));
  resource = cache.ObtainResource(key);
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
}

TEST(ResourceCache, StoreWithPurge) {
  skity::testing::TestResourceCache cache(
      std::make_unique<skity::testing::TestResourceAllocator>(), 1000);

  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
  skity::testing::TestResourceKey key{100};

  auto resource1 = cache.ObtainResource(key);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  auto resource2 = cache.ObtainResource(key);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  auto resource3 = cache.ObtainResource(key);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(1200));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  cache.StoreResource(resource1);  // trigger purge
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  cache.StoreResource(resource2);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(400));

  cache.StoreResource(resource3);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(800));
}

TEST(ResourceCache, ObtainWithPurge) {
  skity::testing::TestResourceCache cache(
      std::make_unique<skity::testing::TestResourceAllocator>(), 1000);

  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
  skity::testing::TestResourceKey key{100};

  auto resource1 = cache.ObtainResource(key);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  auto resource2 = cache.ObtainResource(key);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  cache.StoreResource(resource1);
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(400));

  skity::testing::TestResourceKey key2{200};
  auto resource3 = cache.ObtainResource(key2);  // trigger purge
  cache.PurgeAsNeeded();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(1200));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
}

TEST(ResourceCache, Pool) {
  skity::testing::TestResourceCache cache(
      std::make_unique<skity::testing::TestResourceAllocator>(), 1000);

  auto pool =
      std::make_unique<skity::testing::TestResourceCache::Pool>((&cache));

  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
  skity::testing::TestResourceKey key{100};

  auto resource1 = cache.ObtainResource(key, pool.get());
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  auto resource2 = cache.ObtainResource(key, pool.get());
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  pool.reset();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(800));
}

TEST(ResourceCache, SetMaxBytes) {
  skity::testing::TestResourceCache cache(
      std::make_unique<skity::testing::TestResourceAllocator>(), 1000);
  auto pool =
      std::make_unique<skity::testing::TestResourceCache::Pool>((&cache));

  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
  skity::testing::TestResourceKey key{100};

  auto resource1 = cache.ObtainResource(key, pool.get());
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  auto resource2 = cache.ObtainResource(key, pool.get());
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  pool.reset();
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(800));

  cache.SetMaxBytes(600);
  EXPECT_EQ(cache.GetMaxbytes(), static_cast<size_t>(600));
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(400));

  cache.SetMaxBytes(200);
  EXPECT_EQ(cache.GetMaxbytes(), static_cast<size_t>(200));
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
}

TEST(ResourceCache, PurgeResourcesByOrder) {
  skity::testing::TestResourceCache cache(
      std::make_unique<skity::testing::TestResourceAllocator>(), 4000);

  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
  skity::testing::TestResourceKey key1{100};
  auto resource1 = cache.ObtainResource(key1);
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  skity::testing::TestResourceKey key2{200};
  auto resource2 = cache.ObtainResource(key2);
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(1200));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  skity::testing::TestResourceKey key3{300};
  auto resource3 = cache.ObtainResource(key3);
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(2400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  skity::testing::TestResourceKey key4{400};
  auto resource4 = cache.ObtainResource(key4);
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(4000));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));

  cache.StoreResource(resource3);
  cache.StoreResource(resource1);
  cache.StoreResource(resource2);
  cache.StoreResource(resource4);

  cache.SetMaxBytes(3000);  // resource3 will be removed.
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(2800));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(2800));

  cache.SetMaxBytes(2500);  // resource1 will be removed.
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(2400));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(2400));

  cache.SetMaxBytes(2000);  // resource2 will be removed.
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(1600));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(1600));

  cache.SetMaxBytes(1000);  // resource4 will be removed.
  EXPECT_EQ(cache.GetTotalResourceBytes(), static_cast<size_t>(0));
  EXPECT_EQ(cache.GetPurgableBytes(), static_cast<size_t>(0));
}
