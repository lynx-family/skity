---
name: "perfetto-tracing"
description: "Guides the capture and optimization of Perfetto traces in benchmarks. Invoke when the user wants to trace performance, analyze bottlenecks, or generate pftrace files."
---

# Perfetto Tracing Guide

This skill provides instructions on how to enable, capture, and optimize Perfetto tracing for benchmarks in the Skity project. Future agents can rely on this skill to properly configure and run benchmarks when performance tracing is required.

## 1. How to Enable Tracing

The Perfetto SDK is integrated via a CMake option. By default, it is turned `OFF` to ensure zero overhead.

- **Build Command**: Pass `-DSKITY_BENCH_ENABLE_PERFETTO=ON` to CMake when configuring the project.
- **Run Benchmark**: Execute the compiled benchmark binary (e.g., `./test/bench/skity_hw_render_bench`).
- **Trace Output**: The session will automatically save the trace file to `output/hw_benchmarks.pftrace`.
- **Verify**: Look for the log "Perfetto Tracing is ENABLED" in the terminal.
- **Analyze**: Open [ui.perfetto.dev](https://ui.perfetto.dev/) in the browser and drag the `.pftrace` file into it.
- **Identification**:
  - **Process**: Look for `skity_hw_bench`.
  - **Thread**: Look for `main_bench_thread`.
  - **Benchmark**: The outer scope event is named after the benchmark (e.g., `Tiger_2000_Metal_MSAA`).
  - **Iteration**: Each loop cycle is marked as `Iteration_N`.

## 2. When to Invoke This Skill
This skill should be invoked when:
- Analyzing benchmark performance bottlenecks.
- The user requests a "trace" or "profile" of a specific rendering operation.
- Debugging unexpected latency in the rendering pipeline.

## 3. Strategies for Long Traces

When dealing with long-running benchmarks or many iterations, trace buffers can easily overflow, resulting in massive trace files or data loss. Use the following strategies to restrict the scope:

### Precise Filtering (Recommended)
Do not run all benchmarks at once. Use the Google Benchmark filter to isolate the target:
```bash
./test/bench/skity_hw_render_bench --benchmark_filter="^TargetBenchmarkName$"
```

### Limit Iterations
Restrict the benchmark iterations to prevent excessive duplicate events from flooding the trace buffer:
```bash
./test/bench/skity_hw_render_bench --benchmark_filter="^TargetBenchmarkName$" --benchmark_min_time=0.1
```
*(Alternatively, use `--benchmark_repetitions=1` if supported).*

### Buffer Optimization
If the trace is still too large or losing data, check the `perfetto::TraceConfig` in the `StartPerfettoSession()` function of `hw_benchmarks.cc`. 
- Ensure the buffer size is adequate (e.g., `1024 * 1024` for 1GB).
- **Policy**: Use `DISCARD` instead of `RING_BUFFER` when internal Skity tracing is enabled. This prevents overwriting the initial metadata (string mappings), which would cause "sequences dropped" errors in the UI.
- **Important**: Call `tracing_session->FlushBlocking()` before stopping the session.

## 4. Skity Internal Events
Skity internal events are tracked under the `"skity2d"` category. Ensure `RegisterSkityPerfettoHandler()` is correctly initialized in `StartPerfettoSession()` so that internal Skity function durations are captured alongside `Benchmark_Draw` and `Target_Flush`.
