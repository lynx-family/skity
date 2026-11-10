// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <skity/graphic/path.hpp>

namespace {

void BuildPath(skity::Path& path, int count) {
  path.MoveTo(0, 0);
  for (int i = 1; i < count; ++i) {
    path.LineTo(i, i % 3);
  }
  path.Close();
}

void BM_PathBuild(benchmark::State& state) {
  for (auto _ : state) {
    skity::Path path;
    BuildPath(path, state.range(0));
    benchmark::DoNotOptimize(path);
  }
}
BENCHMARK(BM_PathBuild)->Arg(4)->Arg(64)->Arg(1024)->Threads(1)->Threads(8);

void BM_PathBuildCubics(benchmark::State& state) {
  for (auto _ : state) {
    skity::Path path;
    path.MoveTo(0, 0);
    for (int i = 1; i < state.range(0); i += 3) {
      path.CubicTo(i, 1, i + 1, 2, i + 2, 0);
    }
    path.Close();
    benchmark::DoNotOptimize(path);
  }
}
BENCHMARK(BM_PathBuildCubics)->Arg(4)->Arg(64)->Arg(1024);

void BM_PathResetReuse(benchmark::State& state) {
  skity::Path path;
  BuildPath(path, state.range(0));
  for (auto _ : state) {
    path.Reset();
    BuildPath(path, state.range(0));
    benchmark::DoNotOptimize(path);
  }
}
BENCHMARK(BM_PathResetReuse)->Arg(4)->Arg(64)->Arg(1024);

void BM_PathCopy(benchmark::State& state) {
  skity::Path source;
  BuildPath(source, state.range(0));
  for (auto _ : state) {
    skity::Path copy(source);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_PathCopy)->Arg(4)->Arg(64)->Arg(1024);

void BM_PathCopyEdit(benchmark::State& state) {
  skity::Path source;
  BuildPath(source, state.range(0));
  for (auto _ : state) {
    skity::Path copy(source);
    copy.SetLastPt(50, 60);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_PathCopyEdit)->Arg(4)->Arg(64)->Arg(1024);

void BM_PathCopyAppend(benchmark::State& state) {
  skity::Path source;
  BuildPath(source, state.range(0));
  for (auto _ : state) {
    skity::Path copy(source);
    copy.CubicTo(10, 20, 30, 40, 50, 60);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_PathCopyAppend)->Arg(4)->Arg(64)->Arg(1024);

void BM_PathResetPublish(benchmark::State& state) {
  skity::Path path;
  for (auto _ : state) {
    path.Reset();
    BuildPath(path, state.range(0));
    skity::Path recorded(path);
    benchmark::DoNotOptimize(recorded);
  }
}
BENCHMARK(BM_PathResetPublish)->Arg(4)->Arg(64)->Arg(1024);

}  // namespace
