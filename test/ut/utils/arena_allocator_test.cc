// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/utils/arena_allocator.hpp"

#include <gtest/gtest.h>
#include <stdint.h>

#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

class TestAllocator : public skity::Allocator {
 public:
  TestAllocator(std::function<void()> alloc_callback,
                std::function<void()> free_callback)
      : alloc_callback(alloc_callback), free_callback(free_callback) {}

  skity::Block Alloc(size_t size) override {
    alloc_callback();
    return internal.Alloc(size);
  }

  void Free(skity::Block& block) override {
    free_callback();
    return internal.Free(block);
  }

  skity::DefaultAllocator internal;
  std::function<void()> alloc_callback;
  std::function<void()> free_callback;
};

TEST(Arena, AllocateZeroByte) {
  skity::Arena arena;
  EXPECT_DEATH(arena.Allocate(0, 1), "");
  EXPECT_DEATH(arena.Allocate(0, 4), "");
}

TEST(Arena, NotAlignedToPowersOfTwo) {
  skity::Arena arena;
  EXPECT_DEATH(arena.Allocate(12, 3), "");
  EXPECT_DEATH(arena.Allocate(20, 6), "");
}

TEST(Arena, Reset) {
  int count = 0;
  auto alloc_func = [&count]() mutable { count++; };
  auto free_func = [&count]() mutable { count--; };
  std::shared_ptr<TestAllocator> allocator =
      std::make_shared<TestAllocator>(alloc_func, free_func);
  skity::Arena arena(skity::kDefaultBlockSize, allocator);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(0));
  EXPECT_EQ(arena.GetCursor(), nullptr);
  EXPECT_EQ(arena.GetEnd(), nullptr);
  EXPECT_EQ(count, 0);

  uint8_t* ptr = arena.Allocate(10, 8);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_NE(arena.GetCursor(), nullptr);
  EXPECT_NE(arena.GetEnd(), nullptr);
  EXPECT_EQ(count, 1);

  arena.Reset();
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(0));
  EXPECT_EQ(arena.GetCursor(), nullptr);
  EXPECT_EQ(arena.GetEnd(), nullptr);
  EXPECT_EQ(count, 0);
}

TEST(Arena, Allocate) {
  skity::Arena arena;
  uint8_t* ptr;
  ptr = arena.Allocate(10, 8);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(ptr - arena.GetBlocks().back().head, 0);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks().back().head, 10);
  EXPECT_EQ(arena.GetEnd(),
            arena.GetBlocks().back().head + arena.GetBlockSize());
  EXPECT_EQ(arena.GetBlockSize(), static_cast<size_t>(4096));

  ptr = arena.Allocate(10, 8);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(ptr - arena.GetBlocks().back().head, 16);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks().back().head, 26);  // 16 + 10

  ptr = arena.Allocate(10, 4);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(ptr - arena.GetBlocks().back().head, 28);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks().back().head, 38);  // 28 + 10

  ptr = arena.Allocate(35, 16);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(ptr - arena.GetBlocks().back().head, 48);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks().back().head, 83);  // 48 + 35

  // A new block will be created, but subsequent memory will still be allocated
  // from the old block.
  ptr = arena.Allocate(5000, 16);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(2));
  EXPECT_EQ(ptr - arena.GetBlocks()[1].head, 0);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[0].head, 83);
  EXPECT_EQ(arena.GetEnd(), arena.GetBlocks()[0].head + arena.GetBlockSize());

  ptr = arena.Allocate(2000, 16);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(2));
  EXPECT_EQ(ptr - arena.GetBlocks()[0].head, 96);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[0].head, 2096);  // 96 + 2000
  EXPECT_EQ(arena.GetEnd(), arena.GetBlocks()[0].head + arena.GetBlockSize());

  // A new block will be created, and subsequent memory will be allocated from
  // the new block
  ptr = arena.Allocate(3000, 16);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(3));
  EXPECT_EQ(ptr - arena.GetBlocks()[2].head, 0);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[2].head, 3000);
  EXPECT_EQ(arena.GetEnd(), arena.GetBlocks()[2].head + arena.GetBlockSize());

  ptr = arena.Allocate(9, 1);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(3));
  EXPECT_EQ(ptr - arena.GetBlocks()[2].head, 3000);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[2].head, 3009);  // 3000 + 9
  EXPECT_EQ(arena.GetEnd(), arena.GetBlocks()[2].head + arena.GetBlockSize());
}

TEST(Arena, FreeAllBlocksOnDestruction) {
  int count = 0;
  auto alloc_func = [&count]() mutable { count++; };
  auto free_func = [&count]() mutable { count--; };
  std::shared_ptr<TestAllocator> allocator =
      std::make_shared<TestAllocator>(alloc_func, free_func);
  {
    skity::Arena arena{4096, allocator};
    arena.Allocate(300, 16);
    EXPECT_EQ(count, 1);
    arena.Allocate(2000, 32);
    EXPECT_EQ(count, 1);
    arena.Allocate(3000, 32);
    EXPECT_EQ(count, 2);
    arena.Allocate(5000, 32);
    EXPECT_EQ(count, 3);
  }
  // All blocks have been freed
  EXPECT_EQ(count, 0);
}

namespace {
struct Foo {
  int32_t a;
  int64_t b;
  float c;
  double d;
};

static_assert(offsetof(Foo, a) == static_cast<size_t>(0));
static_assert(offsetof(Foo, b) == static_cast<size_t>(8));
static_assert(offsetof(Foo, c) == static_cast<size_t>(16));
static_assert(offsetof(Foo, d) == static_cast<size_t>(24));
static_assert(sizeof(Foo) == static_cast<size_t>(32));
static_assert(std::is_trivial<Foo>::value);
static_assert(std::is_standard_layout<Foo>::value);
static_assert(std::is_trivially_destructible<Foo>::value);

struct Bar {
  int32_t a = 15;
  int64_t b;
  float c = 3.3f;
  double d;
};
static_assert(offsetof(Bar, a) == static_cast<size_t>(0));
static_assert(offsetof(Bar, b) == static_cast<size_t>(8));
static_assert(offsetof(Bar, c) == static_cast<size_t>(16));
static_assert(offsetof(Bar, d) == static_cast<size_t>(24));
static_assert(sizeof(Bar) == static_cast<size_t>(32));
static_assert(!std::is_trivial<Bar>::value);
static_assert(std::is_standard_layout<Bar>::value);
static_assert(std::is_trivially_destructible<Bar>::value);

struct Baz {
  int32_t a;
  int64_t b;
  float c;

 protected:
  double d;
};
static_assert(offsetof(Baz, a) == static_cast<size_t>(0));
static_assert(offsetof(Baz, b) == static_cast<size_t>(8));
static_assert(offsetof(Baz, c) == static_cast<size_t>(16));
static_assert(sizeof(Baz) == static_cast<size_t>(32));
static_assert(std::is_trivial<Baz>::value);
static_assert(!std::is_standard_layout<Baz>::value);
static_assert(std::is_trivially_destructible<Baz>::value);

struct Qux {
  int32_t a;
  int64_t b;
  std::string c;
};

static_assert(offsetof(Qux, a) == static_cast<size_t>(0));
static_assert(offsetof(Qux, b) == static_cast<size_t>(8));
static_assert(offsetof(Qux, c) == static_cast<size_t>(16));
static_assert(sizeof(Qux) == static_cast<size_t>(40));
static_assert(alignof(Qux) == static_cast<size_t>(8));
static_assert(!std::is_trivial<Qux>::value);
static_assert(std::is_standard_layout<Qux>::value);
static_assert(!std::is_trivially_destructible<Qux>::value);

TEST(ArenaAllocator, Make) {
  skity::ArenaAllocator arena_allocator;
  auto& arena = arena_allocator.GetArena();
  Foo* foo = arena_allocator.Make<Foo>();  // 32 bytes;
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(foo) - arena.GetBlocks()[0].head, 0);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[0].head, 32);
  EXPECT_EQ(arena_allocator.GetFinalizerHead(), nullptr);

  Bar* bar = arena_allocator.Make<Bar>();  // 32 bytes;
  EXPECT_EQ(bar->a, 15);
  EXPECT_EQ(bar->c, 3.3f);
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(bar) - arena.GetBlocks()[0].head, 32);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[0].head, 64);
  EXPECT_EQ(arena_allocator.GetFinalizerHead(), nullptr);

  Baz* baz = arena_allocator.Make<Baz>();  // 32 bytes;
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(baz) - arena.GetBlocks()[0].head, 64);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[0].head, 96);
  EXPECT_EQ(arena_allocator.GetFinalizerHead(), nullptr);

  Qux* qux = arena_allocator.Make<Qux>();  // 40 bytes;
  EXPECT_EQ(arena.GetBlocks().size(), static_cast<size_t>(1));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(qux) - arena.GetBlocks()[0].head, 96);
  EXPECT_EQ(arena.GetCursor() - arena.GetBlocks()[0].head,
            160);  // 96 + 40 + 24, sizeof(Finalizer) == 24
  EXPECT_EQ(arena_allocator.GetFinalizersCount(), static_cast<size_t>(1));
}

struct Obj {
  Obj(int32_t value, std::function<void(int32_t value)> func)
      : value(value), func(func) {}

  int32_t value;
  ~Obj() { func(value); }

  std::function<void(int32_t value)> func;
};

TEST(ArenaAllocator, Finalize) {
  std::vector<int32_t> array;
  auto func = [&array](int32_t value) mutable { array.push_back(value); };
  {
    skity::ArenaAllocator arena_allocator;
    auto a = arena_allocator.Make<Obj>(1, func);
    EXPECT_NE(a, nullptr);
    auto b = arena_allocator.Make<Obj>(2, func);
    EXPECT_NE(b, nullptr);
    auto c = arena_allocator.Make<Obj>(3, func);
    EXPECT_NE(c, nullptr);
  }

  EXPECT_EQ(array[0], 3);
  EXPECT_EQ(array[1], 2);
  EXPECT_EQ(array[2], 1);
}

struct Base {
  Base(int32_t value, std::function<void(int32_t value)> func)
      : value(value), func(func) {}

  int32_t value;
  std::function<void(int32_t value)> func;
};

struct A : Base {
  A(int32_t value, std::function<void(int32_t value)> func)
      : Base(value, func) {}
  ~A() { func(value); }
};

struct B : Base {
  B(int32_t value, std::function<void(int32_t value)> func,
    skity::ArenaAllocator& allocator)
      : Base(value, func), a(allocator.Make<A>(value + 1, func)) {}
  A* a;
  ~B() { func(value); }
};

TEST(ArenaAllocator, Nested) {
  std::vector<int32_t> array;
  auto func = [&array](int32_t value) mutable { array.push_back(value); };
  {
    skity::ArenaAllocator arena_allocator;
    auto b = arena_allocator.Make<B>(1, func, arena_allocator);
    EXPECT_NE(b, nullptr);
    EXPECT_EQ(arena_allocator.GetFinalizersCount(), static_cast<size_t>(2));
  }

  EXPECT_EQ(array[0], 1);  // ~B
  EXPECT_EQ(array[1], 2);  // ~A
}

TEST(ArenaAllocator, Reset) {
  int count = 0;
  auto alloc_func = [&count]() mutable { count++; };
  auto free_func = [&count]() mutable { count--; };
  std::shared_ptr<TestAllocator> allocator =
      std::make_shared<TestAllocator>(alloc_func, free_func);
  skity::ArenaAllocator arena_allocator(allocator);
  EXPECT_EQ(arena_allocator.GetFinalizerHead(), nullptr);
  EXPECT_EQ(count, 0);
  std::vector<int32_t> array;
  auto func = [&array](int32_t value) mutable { array.push_back(value); };
  auto obj = arena_allocator.Make<Obj>(1, func);
  EXPECT_NE(obj, nullptr);
  EXPECT_NE(arena_allocator.GetFinalizerHead(), nullptr);
  EXPECT_EQ(count, 1);
  arena_allocator.Reset();
  EXPECT_EQ(arena_allocator.GetFinalizerHead(), nullptr);
  EXPECT_EQ(count, 0);
}

TEST(BlockCacheAllocator, CacheAndReuse) {
  int count = 0;
  auto alloc_func = [&count]() mutable { count++; };
  auto free_func = [&count]() mutable { count--; };
  std::shared_ptr<TestAllocator> allocator =
      std::make_shared<TestAllocator>(alloc_func, free_func);
  {
    auto block_cache_allocator =
        std::make_shared<skity::BlockCacheAllocator>(allocator);
    EXPECT_EQ(block_cache_allocator->GetBlocks().size(),
              static_cast<size_t>(0));
    EXPECT_EQ(count, 0);
    {
      auto arena_allocator = skity::ArenaAllocator(block_cache_allocator);
      Foo* foo = arena_allocator.Make<Foo>();
      Bar* bar = arena_allocator.Make<Bar>();
      EXPECT_NE(foo, nullptr);
      EXPECT_NE(bar, nullptr);
    }
    EXPECT_EQ(block_cache_allocator->GetBlocks().size(),
              static_cast<size_t>(1));
    EXPECT_EQ(count, 1);
    {
      auto arena_allocator = skity::ArenaAllocator(block_cache_allocator);
      Foo* foo = arena_allocator.Make<Foo>();
      Bar* bar = arena_allocator.Make<Bar>();
      EXPECT_NE(foo, nullptr);
      EXPECT_NE(bar, nullptr);
      EXPECT_EQ(block_cache_allocator->GetBlocks().size(),
                static_cast<size_t>(0));
      EXPECT_EQ(count, 1);
    }
    EXPECT_EQ(block_cache_allocator->GetBlocks().size(),
              static_cast<size_t>(1));
    EXPECT_EQ(count, 1);
  }
  EXPECT_EQ(count, 0);
}

TEST(BlockCacheAllocator, DontCacheBigBlock) {
  int count = 0;
  auto alloc_func = [&count]() mutable { count++; };
  auto free_func = [&count]() mutable { count--; };
  std::shared_ptr<TestAllocator> allocator =
      std::make_shared<TestAllocator>(alloc_func, free_func);
  auto block_cache_allocator =
      std::make_shared<skity::BlockCacheAllocator>(allocator);
  EXPECT_EQ(block_cache_allocator->GetBlocks().size(), static_cast<size_t>(0));

  {
    skity::Arena arena(skity::kDefaultBlockSize, block_cache_allocator);
    arena.Allocate(2 * skity::kDefaultBlockSize, 8);
    EXPECT_EQ(count, 1);
  }
  EXPECT_EQ(block_cache_allocator->GetBlocks().size(), static_cast<size_t>(0));
  EXPECT_EQ(count, 0);
}

}  // namespace
