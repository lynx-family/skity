// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <skity/skity.hpp>

#include "src/render/hw/hw_path_raster.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/utils/vector_cache.hpp"

enum PathIndex : int64_t {
  kBigCircle = 0,
  kLines = 1,
  kBigQuad = 2,
  kBigConic = 3,
  kBigCubic = 4,
};

static skity::Path GetPathByIndex(PathIndex index) {
  switch (index) {
    case PathIndex::kBigCircle: {
      skity::Path path;
      path.AddCircle(500, 500, 500);
      return path;
    }
    case PathIndex::kLines: {
      skity::Path path;
      path.MoveTo(0, 0);
      path.LineTo(0, 1000);
      path.LineTo(1000, 1000);
      return path;
    }
    case PathIndex::kBigQuad: {
      skity::Path path;
      path.MoveTo(0, 0);
      path.QuadTo(0, 1000, 1000, 1000);
      path.LineTo(0, 0);
      return path;
    }
    case PathIndex::kBigConic: {
      skity::Path path;
      path.MoveTo(0, 0);
      path.ConicTo(0, 1000, 1000, 1000, FloatRoot2Over2);
      path.LineTo(0, 0);
      return path;
    }
    case PathIndex::kBigCubic: {
      skity::Path path;
      path.MoveTo(0, 500);
      path.CubicTo(250, 0, 750, 1000, 1000, 500);
      path.LineTo(0, 500);
      return path;
    }
  }
}

static void BM_HWPathFillRaster_FillPath(benchmark::State& state) {
  skity::Paint paint;
  skity::Matrix matrix;
  skity::VectorCache<float> vertex_vector_cache;
  skity::VectorCache<uint32_t> index_vector_cache;
  skity::HWStageBuffer buffer{nullptr, nullptr, nullptr, 256};

  skity::Path path = GetPathByIndex(static_cast<PathIndex>(state.range(0)));
  for (auto _ : state) {
    for (auto i = 0; i < 1000; i++) {
      skity::HWPathFillRaster raster(paint, matrix, &vertex_vector_cache,
                                     &index_vector_cache);
      raster.FillPath(path);
      buffer.Push(const_cast<float*>(raster.GetRawVertexBuffer().data()),
                  raster.GetRawVertexBuffer().size() * sizeof(float));

      buffer.PushIndex(const_cast<uint32_t*>(raster.GetRawIndexBuffer().data()),
                       raster.GetRawIndexBuffer().size() * sizeof(uint32_t));
    }
  }
}

BENCHMARK(BM_HWPathFillRaster_FillPath)
    ->Args({
        PathIndex::kBigCircle,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_FillPath)
    ->Args({
        PathIndex::kLines,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_FillPath)
    ->Args({
        PathIndex::kBigQuad,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_FillPath)
    ->Args({
        PathIndex::kBigConic,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_FillPath)
    ->Args({
        PathIndex::kBigCubic,
    })
    ->Unit(benchmark::kMicrosecond);

static void BM_HWPathFillRaster_StrokePath(benchmark::State& state) {
  skity::Paint paint;
  skity::Matrix matrix;
  skity::VectorCache<float> vertex_vector_cache;
  skity::VectorCache<uint32_t> index_vector_cache;

  skity::HWStageBuffer buffer{nullptr, nullptr, nullptr, 256};

  skity::Path path = GetPathByIndex(static_cast<PathIndex>(state.range(0)));
  paint.SetStrokeWidth(state.range(1));
  paint.SetStrokeCap(static_cast<skity::Paint::Cap>(state.range(2)));
  paint.SetStrokeJoin(static_cast<skity::Paint::Join>(state.range(3)));

  for (auto _ : state) {
    for (auto i = 0; i < 1000; i++) {
      skity::HWPathStrokeRaster raster(paint, matrix, &vertex_vector_cache,
                                       &index_vector_cache);
      raster.StrokePath(path);
      buffer.Push(const_cast<float*>(raster.GetRawVertexBuffer().data()),
                  raster.GetRawVertexBuffer().size() * sizeof(float));

      buffer.PushIndex(const_cast<uint32_t*>(raster.GetRawIndexBuffer().data()),
                       raster.GetRawIndexBuffer().size() * sizeof(uint32_t));
    }
  }
}
BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigCircle,
        10,
        skity::Paint::kRound_Cap,
        skity::Paint::kRound_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigCircle,
        10,
        skity::Paint::kButt_Cap,
        skity::Paint::kMiter_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kLines,
        10,
        skity::Paint::kRound_Cap,
        skity::Paint::kRound_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kLines,
        10,
        skity::Paint::kButt_Cap,
        skity::Paint::kMiter_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigQuad,
        10,
        skity::Paint::kRound_Cap,
        skity::Paint::kRound_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigQuad,
        10,
        skity::Paint::kButt_Cap,
        skity::Paint::kMiter_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigConic,
        10,
        skity::Paint::kRound_Cap,
        skity::Paint::kRound_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigConic,
        10,
        skity::Paint::kButt_Cap,
        skity::Paint::kMiter_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigCubic,
        10,
        skity::Paint::kRound_Cap,
        skity::Paint::kRound_Join,
    })
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathFillRaster_StrokePath)
    ->Args({
        PathIndex::kBigCubic,
        10,
        skity::Paint::kButt_Cap,
        skity::Paint::kMiter_Join,
    })
    ->Unit(benchmark::kMicrosecond);
