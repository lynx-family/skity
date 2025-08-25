// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <skity/skity.hpp>
#include <sstream>

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
namespace {
std::string GetFillLabel(PathIndex index) {
  switch (index) {
    case PathIndex::kBigCircle:
      return "BigCircle";
    case PathIndex::kLines:
      return "Lines";
    case PathIndex::kBigQuad:
      return "BigQuad";
    case PathIndex::kBigConic:
      return "BigConic";
    case PathIndex::kBigCubic:
      return "BigCubic";
  }
}

std::string GetStrokeLabel(PathIndex index, float stroke_width,
                           skity::Paint::Cap cap, skity::Paint::Join join) {
  std::stringstream ss;
  switch (index) {
    case PathIndex::kBigCircle:
      ss << "BigCircle";
      break;
    case PathIndex::kLines:
      ss << "Lines";
      break;
    case PathIndex::kBigQuad:
      ss << "BigQuad";
      break;
    case PathIndex::kBigConic:
      ss << "BigConic";
      break;
    case PathIndex::kBigCubic:
      ss << "BigCubic";
      break;
  }

  ss << "_";

  switch (cap) {
    case skity::Paint::kButt_Cap:
      ss << "ButtCap";
      break;
    case skity::Paint::kRound_Cap:
      ss << "RoundCap";
      break;
    case skity::Paint::kSquare_Cap:
      ss << "SquareCap";
      break;
  }

  ss << "_";

  switch (join) {
    case skity::Paint::kMiter_Join:
      ss << "MiterJoin";
      break;
    case skity::Paint::kRound_Join:
      ss << "RoundJoin";
      break;
    case skity::Paint::kBevel_Join:
      ss << "BevelJoin";
      break;
  }

  return ss.str();
}

}  // namespace

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

static void BM_HWPathRaster_FillPath(benchmark::State& state) {
  state.SetLabel(GetFillLabel(static_cast<PathIndex>(state.range(0))));

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

BENCHMARK(BM_HWPathRaster_FillPath)
    ->ArgsProduct({{kBigCircle, kLines, kBigQuad, kBigConic, kBigCubic}})
    ->ArgNames({"type"})
    ->Unit(benchmark::kMicrosecond);

static void BM_HWPathRaster_StrokePath(benchmark::State& state) {
  state.SetLabel(
      GetStrokeLabel(static_cast<PathIndex>(state.range(0)), state.range(1),
                     static_cast<skity::Paint::Cap>(state.range(2)),
                     static_cast<skity::Paint::Join>(state.range(3))));

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

BENCHMARK(BM_HWPathRaster_StrokePath)
    ->ArgsProduct({
        // type
        {kBigCircle, kLines, kBigQuad, kBigConic, kBigCubic},
        // stroke_width
        {10},
        // cap
        {skity::Paint::kRound_Cap},
        // join
        {skity::Paint::kRound_Join},
    })
    ->ArgNames({"type", "stroke_width", "cap", "join"})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HWPathRaster_StrokePath)
    ->ArgsProduct({
        // type
        {kBigCircle, kLines, kBigQuad, kBigConic, kBigCubic},
        // stroke_width
        {10},
        // cap
        {skity::Paint::kButt_Cap},
        // join
        {skity::Paint::kMiter_Join},
    })
    ->ArgNames({"type", "stroke_width", "cap", "join"})
    ->Unit(benchmark::kMicrosecond);
