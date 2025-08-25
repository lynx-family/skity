// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <random>
#include <skity/skity.hpp>

static void BM_MatrixMultiply(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
  skity::Matrix m[1000];

  for (int32_t i = 0; i < 1000; i++) {
    m[i] = skity::Matrix(dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng));
  }

  for (auto _ : state) {
    skity::Matrix result;
    for (int32_t i = 0; i < 1000; i++) {
      result = result * m[i];
    }
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_MatrixMultiply)->Unit(benchmark::kMicrosecond);

static void BM_MatrixInvert(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
  skity::Matrix m[1000];
  skity::Matrix inv[1000];

  for (int32_t i = 0; i < 1000; i++) {
    m[i] = skity::Matrix(dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng));
  }

  for (auto _ : state) {
    for (int32_t i = 0; i < 1000; i++) {
      m[i].Invert(&inv[i]);
    }
  }
}
BENCHMARK(BM_MatrixInvert)->Unit(benchmark::kMicrosecond);

static void BM_MatrixInvertAffine(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
  skity::Matrix m[1000];
  skity::Matrix inv[1000];

  for (int32_t i = 0; i < 1000; i++) {
    m[i] = skity::Matrix(dist(rng), dist(rng), 0, 0,   //
                         dist(rng), dist(rng), 0, 0,   //
                         0, 0, 1, 0,                   //
                         dist(rng), dist(rng), 0, 1);  //
  }

  for (auto _ : state) {
    for (int32_t i = 0; i < 1000; i++) {
      m[i].Invert(&inv[i]);
    }
  }
}
BENCHMARK(BM_MatrixInvertAffine)->Unit(benchmark::kMicrosecond);

static void BM_MatrixMapRect(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
  skity::Matrix m[1000];
  skity::Rect src[1000];
  skity::Rect dst[1000];

  for (int32_t i = 0; i < 1000; i++) {
    m[i] = skity::Matrix(dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng));
  }

  std::uniform_real_distribution<float> dist2(.0f, 1000.0f);
  for (int32_t i = 0; i < 1000; i++) {
    src[i] =
        skity::Rect::MakeXYWH(dist2(rng), dist2(rng), dist2(rng), dist2(rng));
  }

  for (auto _ : state) {
    for (int32_t i = 0; i < 1000; i++) {
      m[i].MapRect(&dst[i], src[i]);
    }
  }
}
BENCHMARK(BM_MatrixMapRect)->Unit(benchmark::kMicrosecond);

static void BM_MatrixMapPoints(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
  skity::Matrix m[1000];
  skity::Point src[1000];
  skity::Point dst[1000];

  for (int32_t i = 0; i < 1000; i++) {
    m[i] = skity::Matrix(dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng));
  }

  std::uniform_real_distribution<float> dist2(-1000.0f, 1000.0f);
  for (int32_t i = 0; i < 1000; i++) {
    src[i] = skity::Point(dist2(rng), dist2(rng), dist2(rng), dist2(rng));
  }

  for (auto _ : state) {
    for (int32_t i = 0; i < 1000; i++) {
      m[i].MapPoints(dst, src, 1);
    }
  }
}
BENCHMARK(BM_MatrixMapPoints)->Unit(benchmark::kMicrosecond);

static void BM_MatrixMapPoints2(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
  skity::Matrix m[1000];
  skity::Vec2 src[1000];
  skity::Vec2 dst[1000];

  for (int32_t i = 0; i < 1000; i++) {
    m[i] = skity::Matrix(dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng), dist(rng), dist(rng), dist(rng), dist(rng),
                         dist(rng));
  }

  std::uniform_real_distribution<float> dist2(-1000.0f, 1000.0f);
  for (int32_t i = 0; i < 1000; i++) {
    src[i] = skity::Vec2(dist2(rng), dist2(rng));
  }

  for (auto _ : state) {
    for (int32_t i = 0; i < 1000; i++) {
      m[i].MapPoints(dst, src, 1);
    }
  }
}
BENCHMARK(BM_MatrixMapPoints2)->Unit(benchmark::kMicrosecond);
