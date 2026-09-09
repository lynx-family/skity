// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/base/lru_cache.hpp"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace skity {
namespace {

struct Key {
  char value;

  size_t hash() const { return value; }
  bool operator==(const Key& other) const { return value == other.value; }
  bool operator!=(const Key& other) const { return !(*this == other); }
};

TEST(LRUCacheTest, FindPromotesEntry) {
  LRUCache<Key, int> cache(2);
  EXPECT_EQ(cache.Find({'A'}), nullptr);
  auto* a = cache.Insert({'A'}, 1);
  cache.Insert({'B'}, 2);
  EXPECT_EQ(cache.Find({'A'}), a);
  EXPECT_EQ(cache.Find({'A'}), a);
  cache.Insert({'C'}, 3);
  EXPECT_EQ(cache.Find({'B'}), nullptr);
  EXPECT_EQ(cache.Find({'A'}), a);
  ASSERT_NE(cache.Find({'C'}), nullptr);
  EXPECT_EQ(*cache.Find({'C'}), 3);
}

TEST(LRUCacheTest, DuplicateInsertUpdatesAndPromotesEntry) {
  LRUCache<Key, int> cache(2);
  auto* a = cache.Insert({'A'}, 1);
  cache.Insert({'B'}, 2);
  EXPECT_EQ(cache.Insert({'A'}, 10), a);
  EXPECT_EQ(*a, 10);
  EXPECT_THAT(cache.CollectKeys(),
              testing::UnorderedElementsAre(Key{'A'}, Key{'B'}));
  cache.Insert({'C'}, 3);
  EXPECT_FALSE(cache.Exsit({'B'}));
  EXPECT_TRUE(cache.Exsit({'A'}));
  EXPECT_TRUE(cache.Exsit({'C'}));
  cache.Remove({'A'});
  cache.Insert({'D'}, 4);
  cache.Insert({'E'}, 5);
  EXPECT_THAT(cache.CollectKeys(),
              testing::UnorderedElementsAre(Key{'D'}, Key{'E'}));
}

TEST(LRUCacheTest, EvictsLeastRecentlyUsedEntry) {
  LRUCache<Key, int> cache(2);
  cache.Insert({'A'}, 1);
  cache.Insert({'B'}, 2);
  EXPECT_TRUE(cache.Exsit({'A'}));
  EXPECT_THAT(cache.CollectKeys(),
              testing::UnorderedElementsAre(Key{'A'}, Key{'B'}));
  cache.Insert({'C'}, 3);
  EXPECT_EQ(cache.Find({'A'}), nullptr);
  EXPECT_TRUE(cache.Exsit({'B'}));
  EXPECT_TRUE(cache.Exsit({'C'}));
}

TEST(LRUCacheTest, RemovePreservesOtherEntries) {
  LRUCache<Key, int> cache(2);
  cache.Insert({'A'}, 1);
  auto* b = cache.Insert({'B'}, 2);
  cache.Remove({'A'});
  EXPECT_EQ(cache.Find({'A'}), nullptr);
  EXPECT_FALSE(cache.Exsit({'A'}));
  EXPECT_EQ(cache.Find({'B'}), b);
  EXPECT_EQ(*b, 2);
  EXPECT_THAT(cache.CollectKeys(), testing::ElementsAre(Key{'B'}));
  cache.Remove({'B'});
  EXPECT_TRUE(cache.CollectKeys().empty());
  EXPECT_NE(cache.Insert({'C'}, 3), nullptr);
}

TEST(LRUCacheTest, SupportsMoveOnlyValues) {
  LRUCache<Key, std::unique_ptr<int>> cache(2);
  auto* a = cache.Insert({'A'}, std::make_unique<int>(1));
  cache.Insert({'B'}, std::make_unique<int>(2));
  EXPECT_EQ(cache.Insert({'A'}, std::make_unique<int>(10)), a);
  EXPECT_EQ(**a, 10);
  cache.Insert({'C'}, std::make_unique<int>(3));
  EXPECT_EQ(cache.Find({'B'}), nullptr);
  EXPECT_EQ(cache.Find({'A'}), a);
  cache.Remove({'A'});
  ASSERT_NE(cache.Find({'C'}), nullptr);
  EXPECT_EQ(**cache.Find({'C'}), 3);
}

TEST(LRUCacheTest, ReleasesValues) {
  std::weak_ptr<int> last;
  {
    LRUCache<Key, std::shared_ptr<int>> cache(1);
    auto* value = cache.Insert({'A'}, std::make_shared<int>(1));
    std::weak_ptr<int> replaced = *value;
    value = cache.Insert({'A'}, std::make_shared<int>(2));
    EXPECT_TRUE(replaced.expired());
    std::weak_ptr<int> evicted = *value;
    value = cache.Insert({'B'}, std::make_shared<int>(3));
    EXPECT_TRUE(evicted.expired());
    std::weak_ptr<int> removed = *value;
    cache.Remove({'B'});
    EXPECT_TRUE(removed.expired());
    last = *cache.Insert({'C'}, std::make_shared<int>(4));
    EXPECT_FALSE(last.expired());
  }
  EXPECT_TRUE(last.expired());
}

}  // namespace
}  // namespace skity
