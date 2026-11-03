// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_CASE_DRAW_CANVASMARK_CAPI_H
#define TEST_BENCH_CASE_DRAW_CANVASMARK_CAPI_H

// C-API + RAII-wrapper twin of the CanvasMark cases (draw_canvasmark.cc).
// Declared with C linkage so the legacy-API bench TU (hw_benchmarks.cc) can
// call them: the two compilation units resolve <skity/...> includes against
// different header trees and cannot share C++ types.

#ifdef __cplusplus
extern "C" {
#endif

// The case compiles into its own dylib with -fvisibility=hidden; only these
// entry points are exported. Everything else the wrapper headers emit stays
// file-local — see the ODR note in test/bench/CMakeLists.txt.
#if defined(__GNUC__)
#define CANVASMARK_CAPI_EXPORT __attribute__((visibility("default")))
#else
#define CANVASMARK_CAPI_EXPORT
#endif

/** Materialize the Paths scene data once (outside the timed region). */
CANVASMARK_CAPI_EXPORT void CanvasMarkCAPI_PreparePaths(void);

/** Materialize the Suits scene data once (outside the timed region). */
CANVASMARK_CAPI_EXPORT void CanvasMarkCAPI_PrepareSuits(void);

/**
 * Replay the CanvasMark Paths frame through the C ABI + RAII wrapper.
 *
 * @param native_canvas raw pointer value of the legacy skity::Canvas* the
 *        bench harness locked; borrowed via skity_canvas_from_native for the
 *        duration of the call only.
 */
CANVASMARK_CAPI_EXPORT void CanvasMarkCAPI_DrawPaths(void* native_canvas);

/** Same as CanvasMarkCAPI_DrawPaths for the Suits scene. */
CANVASMARK_CAPI_EXPORT void CanvasMarkCAPI_DrawSuits(void* native_canvas);

/**
 * Raw-C twins of the two scenes: the same draw loop written directly against
 * the skity_* entries (no RAII wrapper objects), to split the measured cost
 * between the C ABI layer itself and the wrapper veneer on top of it.
 */
CANVASMARK_CAPI_EXPORT void CanvasMarkCAPI_DrawPathsRaw(void* native_canvas);
CANVASMARK_CAPI_EXPORT void CanvasMarkCAPI_DrawSuitsRaw(void* native_canvas);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TEST_BENCH_CASE_DRAW_CANVASMARK_CAPI_H
