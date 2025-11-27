// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/skity.hpp>
#include <sstream>
#include <vector>

#include "test/bench/case/draw_circle.hpp"
#include "test/bench/common/bench_context.hpp"
#include "test/bench/common/bench_gpu_time_tracer.hpp"
#include "test/bench/common/bench_target.hpp"

namespace fs = std::filesystem;

#define SKITY_BENCH_ALL_GPU_TYPES 1
#define SKITY_BENCH_ALL_AA_TYPES 1
#define SKITY_BENCH_WRITE_PNG 0

fs::path kOutputDir;

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

std::vector<std::vector<int64_t>> ArgsProduct(
    const std::vector<std::vector<int64_t>>& arglists) {
  std::vector<std::vector<int64_t>> output_args;

  std::vector<std::size_t> indices(arglists.size());
  const std::size_t total = std::accumulate(
      std::begin(arglists), std::end(arglists), std::size_t{1},
      [](const std::size_t res, const std::vector<int64_t>& arglist) {
        return res * arglist.size();
      });
  std::vector<int64_t> args;
  args.reserve(arglists.size());
  for (std::size_t i = 0; i < total; i++) {
    for (std::size_t arg = 0; arg < arglists.size(); arg++) {
      args.push_back(arglists[arg][indices[arg]]);
    }
    output_args.push_back(args);
    args.clear();

    std::size_t arg = 0;
    do {
      indices[arg] = (indices[arg] + 1) % arglists[arg].size();
    } while (indices[arg++] == 0 && arg < arglists.size());
  }

  return output_args;
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

  skity::BenchGPUTimeTracer::Instance().SetEnable(
      backend_type == skity::GPUBackendType::kMetal);
  skity::BenchGPUTimeTracer::Instance().ClearFrame();

  for (auto _ : state) {
    state.PauseTiming();
    skity::BenchGPUTimeTracer::Instance().StartTracing();
    skity::BenchGPUTimeTracer::Instance().StartFrame();
    state.ResumeTiming();

    auto canvas = target->LockCanvas();
    benchmark->Draw(canvas, 0);
    target->Flush();

    state.PauseTiming();
    skity::BenchGPUTimeTracer::Instance().EndFrame();
    skity::BenchGPUTimeTracer::Instance().StopTracing();
    state.ResumeTiming();
  }

#if SKITY_BENCH_WRITE_PNG
  context->WriteToFile(target, (kOutputDir / (benchmark->GetName() + "_" +
                                              GetLabel(backend_type, aa)))
                                   .string());
#endif
}

static void RegisterBenchmark(std::shared_ptr<skity::Benchmark> benchmark,
                              skity::GPUBackendType backend_type,
                              skity::BenchTarget::AAType aa) {
  benchmark::RegisterBenchmark(
      (benchmark->GetName() + "_" + GetLabel(backend_type, aa)).c_str(),
      [benchmark, backend_type, aa](benchmark::State& state) {
        RunBenchmark(state, backend_type, aa,
                     [benchmark]() { return benchmark; });
      });
}

static void RegisterFillCircleBenchmark() {
  auto all_args = ArgsProduct({
      // gpu backend type
      GetGPUBackendTypes(),
      // aa
      GetAATypes(),
      // count
      {1, 10, 100, 1000, 10000},
      // radius
      {32, 256},
  });

  for (auto args : all_args) {
    auto backend_type = GetGPUBackendType(args[0]);
    auto aa = GetAAType(args[1]);
    auto count = args[2];
    auto radius = args[3];
    auto benchmark =
        std::make_shared<skity::DrawCircleBenchmark>(count, radius, false);
    RegisterBenchmark(benchmark, backend_type, aa);
  }
}

static void RegisterStrokeCircleBenchmark() {
  auto all_args = ArgsProduct({
      // gpu backend type
      GetGPUBackendTypes(),
      // aa
      GetAATypes(),
      // count
      {1, 10, 100, 1000, 10000},
      // radius
      {32, 256},
  });

  for (auto args : all_args) {
    auto backend_type = GetGPUBackendType(args[0]);
    auto aa = GetAAType(args[1]);
    auto count = args[2];
    auto radius = args[3];
    auto benchmark =
        std::make_shared<skity::DrawCircleBenchmark>(count, radius, false);
    benchmark->SetStroke(true);
    benchmark->SetStrokeWidth(10);
    RegisterBenchmark(benchmark, backend_type, aa);
  }
}

static void RegisterAllBenchmarks() {
  RegisterFillCircleBenchmark();
  RegisterStrokeCircleBenchmark();
}

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);

  fs::path exePath = fs::absolute(argv[0]);
  fs::path exeDir = exePath.parent_path();
  kOutputDir = exeDir / "output";

  if (!fs::exists(kOutputDir)) {
    fs::create_directory(kOutputDir);
  }

  RegisterAllBenchmarks();

  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
