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

namespace fs = std::filesystem;

#ifdef SKITY_BENCH_ENABLE_PERFETTO
#include <skity/utils/trace_event.hpp>

#include "perfetto.h"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("skity2d").SetDescription("Skity Events"));

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

#define SKITY_TRACE_EVENT(name) TRACE_EVENT("skity2d", name)

static void PerfettoTraceBegin(const char* category_group,
                               const char* section_name, int64_t trace_id,
                               const char* arg1_name, const char* arg1_val,
                               const char* arg2_name, const char* arg2_val) {
  if (section_name) {
    if (arg1_name && arg2_name) {
      TRACE_EVENT_BEGIN("skity2d", perfetto::DynamicString(section_name),
                        perfetto::DynamicString(arg1_name),
                        perfetto::DynamicString(arg1_val ? arg1_val : ""),
                        perfetto::DynamicString(arg2_name),
                        perfetto::DynamicString(arg2_val ? arg2_val : ""));
    } else if (arg1_name) {
      TRACE_EVENT_BEGIN("skity2d", perfetto::DynamicString(section_name),
                        perfetto::DynamicString(arg1_name),
                        perfetto::DynamicString(arg1_val ? arg1_val : ""));
    } else if (arg2_name) {
      TRACE_EVENT_BEGIN("skity2d", perfetto::DynamicString(section_name),
                        perfetto::DynamicString(arg2_name),
                        perfetto::DynamicString(arg2_val ? arg2_val : ""));
    } else {
      TRACE_EVENT_BEGIN("skity2d", perfetto::DynamicString(section_name));
    }
  }
}

static void PerfettoTraceEnd(const char* category_group,
                             const char* section_name, int64_t trace_id) {
  TRACE_EVENT_END("skity2d");
}

static void PerfettoTraceCounter(const char* category, const char* name,
                                 uint64_t counter, bool incremental) {
  if (name) {
    TRACE_COUNTER("skity2d",
                  perfetto::CounterTrack{perfetto::DynamicString{name}},
                  counter);
  }
}

static void RegisterSkityPerfettoHandler() {
  skity::SkityTraceHandler handler;
  handler.begin_section = PerfettoTraceBegin;
  handler.end_section = PerfettoTraceEnd;
  handler.counter = PerfettoTraceCounter;
  if (skity::InjectTraceHandler(handler)) {
    std::cout << "Successfully injected Skity Trace Handler" << std::endl;
  }
}

static std::unique_ptr<perfetto::TracingSession> StartPerfettoSession() {
  perfetto::TracingInitArgs args;
  args.backends |= perfetto::kInProcessBackend;
  // Handle high-frequency events by increasing SMB size
  args.shmem_size_hint_kb = 64 * 1024;  // 64MB SMB
  args.shmem_page_size_hint_kb = 32;    // 32KB Pages
  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();

  perfetto::protos::gen::TrackEventConfig track_event_cfg;
  track_event_cfg.add_disabled_categories("*");
  track_event_cfg.add_enabled_categories("skity2d");

  perfetto::TraceConfig cfg;
  auto* buffer_config = cfg.add_buffers();
  buffer_config->set_size_kb(1024 * 1024);  // 1GB Buffer
  buffer_config->set_fill_policy(
      perfetto::protos::gen::TraceConfig_BufferConfig_FillPolicy_DISCARD);

  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");
  ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

  auto tracing_session = perfetto::Tracing::NewTrace();
  tracing_session->Setup(cfg);
  tracing_session->StartBlocking();

  // Set process metadata
  auto process_track = perfetto::ProcessTrack::Current();
  auto desc = process_track.Serialize();
  desc.mutable_process()->set_process_name("skity_hw_bench");
  perfetto::TrackEvent::SetTrackDescriptor(process_track, desc);

  // Set thread metadata for the current thread
  auto thread_track = perfetto::ThreadTrack::Current();
  auto thread_desc = thread_track.Serialize();
  thread_desc.mutable_thread()->set_thread_name("main_bench_thread");
  perfetto::TrackEvent::SetTrackDescriptor(thread_track, thread_desc);

  RegisterSkityPerfettoHandler();
  return tracing_session;
}

static void StopPerfettoSession(perfetto::TracingSession* session,
                                const fs::path& output_path) {
  session->FlushBlocking();
  session->StopBlocking();
  std::vector<char> trace_data(session->ReadTraceBlocking());

  std::ofstream output(output_path.string(), std::ios::out | std::ios::binary);
  output.write(trace_data.data(), trace_data.size());
  output.close();
  std::cout << "Trace saved to: " << output_path.string() << std::endl;
}

#else
#define SKITY_TRACE_EVENT(name)
#endif

#include "test/bench/case/draw_circle.hpp"
#include "test/bench/case/draw_skp.hpp"
#include "test/bench/common/bench_context.hpp"
#include "test/bench/common/bench_gpu_time_tracer.hpp"
#include "test/bench/common/bench_target.hpp"

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

bool IsNumber(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
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

#ifdef SKITY_BENCH_ENABLE_PERFETTO
  // Trace the entire benchmark session
  std::string bench_label =
      benchmark->GetName() + "_" + GetLabel(backend_type, aa);
  SKITY_TRACE_EVENT(perfetto::DynamicString(bench_label));
#endif

  skity::BenchGPUTimeTracer::Instance().SetEnable(
      backend_type == skity::GPUBackendType::kMetal);
  skity::BenchGPUTimeTracer::Instance().ClearFrame();

#ifdef SKITY_BENCH_ENABLE_PERFETTO
  int iteration = 0;
#endif
  for (auto _ : state) {
#ifdef SKITY_BENCH_ENABLE_PERFETTO
    SKITY_TRACE_EVENT(
        perfetto::DynamicString("Iteration_" + std::to_string(iteration++)));
#endif
    state.PauseTiming();
    skity::BenchGPUTimeTracer::Instance().StartTracing();
    skity::BenchGPUTimeTracer::Instance().StartFrame();
    state.ResumeTiming();

    auto canvas = target->LockCanvas();
    {
      SKITY_TRACE_EVENT("Benchmark_Draw");
      benchmark->Draw(canvas, 0);
    }
    {
      SKITY_TRACE_EVENT("Target_Flush");
      target->Flush();
    }

    state.PauseTiming();
    skity::BenchGPUTimeTracer::Instance().EndFrame();
    skity::BenchGPUTimeTracer::Instance().StopTracing();
    context->WaitTillFinished();
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

    auto gradient_benchmark =
        std::make_shared<skity::DrawCircleBenchmark>(count, radius, false);
    gradient_benchmark->SetIsGradient(true);
    RegisterBenchmark(gradient_benchmark, backend_type, aa);
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

    auto gradient_benchmark =
        std::make_shared<skity::DrawCircleBenchmark>(count, radius, false);
    gradient_benchmark->SetStroke(true);
    gradient_benchmark->SetStrokeWidth(10);
    gradient_benchmark->SetIsGradient(true);
    RegisterBenchmark(gradient_benchmark, backend_type, aa);
  }
}

static const char* kTigerSKP = RESOURCES_DIR "/skp/tiger.skp";
static const char* kFlutter01SKP = RESOURCES_DIR "/skp/flutter_01.skp";
static const char* kFlutter02SKP = RESOURCES_DIR "/skp/flutter_02.skp";
static const char* kFlutter03SKP = RESOURCES_DIR "/skp/flutter_03.skp";
static const char* kFlutter04SKP = RESOURCES_DIR "/skp/flutter_04.skp";
static const char* kPrivateDir = RESOURCES_DIR "/skp/private";

static void RegisterTigerSKPBenchmark() {
  auto all_args = ArgsProduct({
      // gpu backend type
      GetGPUBackendTypes(),
      // aa
      GetAATypes(),
  });

  for (auto args : all_args) {
    auto backend_type = GetGPUBackendType(args[0]);
    auto aa = GetAAType(args[1]);
    if (backend_type == skity::GPUBackendType::kOpenGL &&
        aa == skity::BenchTarget::AAType::kMSAA) {
      // OpenGL MSAA performance results on macOS should not be taken as a
      // meaningful benchmark.
      continue;
    }
    auto tiger_1000 = std::make_shared<skity::DrawSKPBenchmark>(
        "Tiger_1000", kTigerSKP, 1000, 1000,
        skity::Matrix::Translate(-130, 20));
    RegisterBenchmark(tiger_1000, backend_type, aa);

    auto tiger_2000 = std::make_shared<skity::DrawSKPBenchmark>(
        "Tiger_2000", kTigerSKP, 2000, 2000,
        skity::Matrix::Scale(2, 2) * skity::Matrix::Translate(-130, 20));
    RegisterBenchmark(tiger_2000, backend_type, aa);

    auto tiger_4000 = std::make_shared<skity::DrawSKPBenchmark>(
        "Tiger_4000", kTigerSKP, 4000, 4000,
        skity::Matrix::Scale(4, 4) * skity::Matrix::Translate(-130, 20));
    RegisterBenchmark(tiger_4000, backend_type, aa);
  }
}

static void RegisterGUIBenchmark() {
  auto all_args = ArgsProduct({
      // gpu backend type
      GetGPUBackendTypes(),
      // aa
      GetAATypes(),
  });

  for (auto args : all_args) {
    auto backend_type = GetGPUBackendType(args[0]);
    auto aa = GetAAType(args[1]);
    if (backend_type == skity::GPUBackendType::kOpenGL &&
        aa == skity::BenchTarget::AAType::kMSAA) {
      // OpenGL MSAA performance results on macOS should not be taken as a
      // meaningful benchmark.
      continue;
    }
    auto flutter_01 = std::make_shared<skity::DrawSKPBenchmark>(
        "Flutter_01", kFlutter01SKP, 1080, 1920, skity::Matrix{});
    RegisterBenchmark(flutter_01, backend_type, aa);

    auto flutter_02 = std::make_shared<skity::DrawSKPBenchmark>(
        "Flutter_02", kFlutter02SKP, 1080, 1920, skity::Matrix{});
    RegisterBenchmark(flutter_02, backend_type, aa);

    auto flutter_03 = std::make_shared<skity::DrawSKPBenchmark>(
        "Flutter_03", kFlutter03SKP, 1080, 1920, skity::Matrix{});
    RegisterBenchmark(flutter_03, backend_type, aa);

    auto flutter_04 = std::make_shared<skity::DrawSKPBenchmark>(
        "Flutter_04", kFlutter04SKP, 1080, 1920, skity::Matrix{});
    RegisterBenchmark(flutter_04, backend_type, aa);

    // If private dir not exists, skip.
    if (!fs::exists(kPrivateDir)) continue;

    // Iterate private dir, find all skp files.
    // Expected filename format: name_width_height.skp
    // Example: MyBenchmark_1080_1920.skp
    for (auto& entry : fs::directory_iterator(kPrivateDir)) {
      if (entry.path().extension() == ".skp") {
        std::string stem = entry.path().stem().string();

        size_t last_underscore = stem.rfind('_');
        if (last_underscore == std::string::npos) continue;

        size_t second_last_underscore = stem.rfind('_', last_underscore - 1);
        if (second_last_underscore == std::string::npos) continue;

        std::string name = stem.substr(0, second_last_underscore);
        std::string w_str =
            stem.substr(second_last_underscore + 1,
                        last_underscore - second_last_underscore - 1);
        std::string h_str = stem.substr(last_underscore + 1);

        if (!IsNumber(w_str) || !IsNumber(h_str)) continue;

        int width = std::stoi(w_str);
        int height = std::stoi(h_str);
        auto benchmark = std::make_shared<skity::DrawSKPBenchmark>(
            name, entry.path().string().c_str(), width, height,
            skity::Matrix{});
        RegisterBenchmark(benchmark, backend_type, aa);
      }
    }
  }
}

static void RegisterAllBenchmarks() {
  RegisterTigerSKPBenchmark();
  RegisterGUIBenchmark();
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

#ifdef SKITY_BENCH_ENABLE_PERFETTO
  auto tracing_session = StartPerfettoSession();
  std::cout << "Perfetto Tracing is ENABLED. Trace file will be saved to: "
            << (kOutputDir / "hw_benchmarks.pftrace").string() << std::endl;
#endif

  RegisterAllBenchmarks();

  benchmark::RunSpecifiedBenchmarks();

#ifdef SKITY_BENCH_ENABLE_PERFETTO
  StopPerfettoSession(tracing_session.get(),
                      kOutputDir / "hw_benchmarks.pftrace");
#endif

  benchmark::Shutdown();
}
