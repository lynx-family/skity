// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/skity.hpp>
#include <sstream>
#include <vector>

#include "test/bench/case/draw_circle.hpp"
#include "test/bench/common/bench_context.hpp"
#include "test/bench/common/bench_target.hpp"

namespace fs = std::filesystem;

#define SKITY_BENCH_ALL_GPU_TYPES 0
#define SKITY_BENCH_ALL_AA_TYPES 0
#define SKITY_BENCH_WRITE_PNG 0

fs::path kOutputDir;

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);

  fs::path exePath = fs::absolute(argv[0]);
  fs::path exeDir = exePath.parent_path();
  kOutputDir = exeDir / "output";

  if (!fs::exists(kOutputDir)) {
    fs::create_directory(kOutputDir);
  }

  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}

using BenchmarkProvider = std::function<std::shared_ptr<skity::Benchmark>()>;

namespace {
skity::GPUBackendType GetGPUBackendType(uint32_t index) {
  switch (index) {
    case 0:
      return skity::GPUBackendType::kMetal;
    case 1:
      return skity::GPUBackendType::kOpenGL;
    default:
      abort();
  }
}

skity::BenchTarget::AAType GetAAType(uint32_t index) {
  switch (index) {
    case 0:
      return skity::BenchTarget::AAType::kNoAA;
    case 1:
      return skity::BenchTarget::AAType::kMSAA;
    case 2:
      return skity::BenchTarget::AAType::kContourAA;
    default:
      abort();
  }
}

std::string GetLabel(skity::GPUBackendType backend_type,
                     skity::BenchTarget::AAType type) {
  std::stringstream ss;
  switch (backend_type) {
    case skity::GPUBackendType::kMetal:
      ss << "Metal";
      break;
    case skity::GPUBackendType::kOpenGL:
      ss << "OpenGL";
      break;
    default:
      abort();
  }

  ss << "_";

  switch (type) {
    case skity::BenchTarget::AAType::kNoAA:
      ss << "NoAA";
      break;
    case skity::BenchTarget::AAType::kMSAA:
      ss << "MSAA";
      break;
    case skity::BenchTarget::AAType::kContourAA:
      ss << "ContourAA";
      break;
    default:
      abort();
  }
  return ss.str();
}

std::vector<int64_t> GetGPUBackendTypes() {
#if SKITY_BENCH_ALL_GPU_TYPES
  return {
      0,  // Metal
      1,  // OpenGL
  };
#else
#ifdef SKITY_BENCH_MTL_BACKEND
  return {
      0,  // Metal
  };
#elif SKITY_BENCH_GL_BACKEND
  return {
      1,  // OpenGL
  };
#endif
#endif
}

std::vector<int64_t> GetAATypes() {
#if SKITY_BENCH_ALL_AA_TYPES
  return {
      0,  // kNoAA
      1,  // kMSAA
      2,  // kContourAA};
  };
#else
  return {
      0,  // kNoAA
  };
#endif
}
}  // namespace

static void RunBenchmark(benchmark::State& state,
                         skity::GPUBackendType backend_type,
                         skity::BenchTarget::AAType aa,
                         BenchmarkProvider provider) {
  state.SetLabel(GetLabel(backend_type, aa));
  auto context = skity::BenchContext::Create(backend_type);
  if (!context) {
    state.SkipWithError("Create BenchContext failed");
    return;
  }
  if (aa == skity::BenchTarget::AAType::kContourAA) {
    context->GetGPUContext()->SetEnableContourAA(true);
  }
  auto benchmark = provider();
  skity::BenchTarget::Options options;
  options.width = benchmark->GetSize().width;
  options.height = benchmark->GetSize().height;
  options.aa = aa;
  auto target = context->CreateTarget(options);

  for (auto _ : state) {
    auto canvas = target->LockCanvas();
    benchmark->Draw(canvas, 0);
    target->Flush();
  }

#if SKITY_BENCH_WRITE_PNG
  context->WriteToFile(target, (kOutputDir / (benchmark->GetName() + "_" +
                                              GetLabel(backend_type, aa)))
                                   .string());
#endif
}

static void BM_FillCircle(benchmark::State& state) {
  RunBenchmark(state, GetGPUBackendType(state.range(0)),
               GetAAType(state.range(1)), [&state]() {
                 return std::make_shared<skity::DrawCircleBenchmark>(
                     state.range(2), state.range(3), false);
               });
}

BENCHMARK(BM_FillCircle)
    ->ArgsProduct({
        // gpu backend type
        GetGPUBackendTypes(),
        // aa
        GetAATypes(),
        // count
        {1, 10, 100, 1000, 10000},
        // radius
        {32, 256},
    })
    ->ArgNames({"gpu", "aa", "count", "radius"})
    ->Unit(benchmark::kMicrosecond);

static void BM_StrokeCircle(benchmark::State& state) {
  RunBenchmark(state, GetGPUBackendType(state.range(0)),
               GetAAType(state.range(1)), [&state]() {
                 auto result = std::make_shared<skity::DrawCircleBenchmark>(
                     state.range(2), state.range(3), false);
                 result->SetStroke(true);
                 result->SetStrokeWidth(state.range(4));
                 return result;
               });
}

BENCHMARK(BM_StrokeCircle)
    ->ArgsProduct({
        // gpu backend type
        GetGPUBackendTypes(),
        // aa
        GetAATypes(),
        // count
        {1, 10, 100, 1000, 10000},
        // radius
        {32, 256},
        // stroke width
        {10},
    })
    ->ArgNames({"gpu", "aa", "count", "radius", "stroke_width"})
    ->Unit(benchmark::kMicrosecond);
