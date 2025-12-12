// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <memory>
#include <skity/skity.hpp>

#include "src/utils/array_list.hpp"

inline static void DoVectorTest(uint32_t times) {
  std::vector<skity::Paint> paints;

  for (uint32_t i = 0; i < times; i++) {
    paints.push_back(std::move(skity::Paint{}));
  }
  uint32_t count;
  for (auto paint : paints) {
    count++;
  }
  assert(count == times);
  benchmark::DoNotOptimize(count);
}

inline static void DoArrayListTest(
    uint32_t times,
    std::shared_ptr<skity::BlockCacheAllocator> block_cache_allocator) {
  skity::ArenaAllocator arena{block_cache_allocator};
  skity::ArrayList<skity::Paint, 32> paints;
  paints.SetArenaAllocator(&arena);
  for (uint32_t i = 0; i < times; i++) {
    paints.push_back(std::move(skity::Paint{}));
  }
  uint32_t count;
  for (auto paint : paints) {
    count++;
  }
  assert(count == times);
  benchmark::DoNotOptimize(count);
}

static void BM_Vector_100(benchmark::State& state) {
  for (auto _ : state) {
    DoVectorTest(100);
  }
}
BENCHMARK(BM_Vector_100);

static void BM_ArrayList_100(benchmark::State& state) {
  auto block_cache_allocator = std::make_shared<skity::BlockCacheAllocator>();
  for (auto _ : state) {
    DoArrayListTest(100, block_cache_allocator);
  }
}
BENCHMARK(BM_ArrayList_100);

static void BM_Vector_1000(benchmark::State& state) {
  for (auto _ : state) {
    DoVectorTest(1000);
  }
}
BENCHMARK(BM_Vector_1000);

static void BM_ArrayList_1000(benchmark::State& state) {
  auto block_cache_allocator = std::make_shared<skity::BlockCacheAllocator>();
  for (auto _ : state) {
    DoArrayListTest(1000, block_cache_allocator);
  }
}
BENCHMARK(BM_ArrayList_1000);

static void BM_Vector_10000(benchmark::State& state) {
  for (auto _ : state) {
    DoVectorTest(10000);
  }
}
BENCHMARK(BM_Vector_10000);

static void BM_ArrayList_10000(benchmark::State& state) {
  auto block_cache_allocator = std::make_shared<skity::BlockCacheAllocator>();
  for (auto _ : state) {
    DoArrayListTest(10000, block_cache_allocator);
  }
}
BENCHMARK(BM_ArrayList_10000);
