# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# Web geometry smoke harness: renders a small set of geometry-only cases
# through the skity WebGPU backend inside a headless browser and compares the
# pixels against checked-in baselines. Driven by tools/web_harness_runner.py.

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR
    "harness/web requires an Emscripten toolchain; configure with emcmake")
endif()

add_executable(skity-web
  cases.cc
  main.cc
)

target_link_libraries(skity-web PRIVATE skity::skity)

target_include_directories(skity-web PRIVATE ${CMAKE_SOURCE_DIR})

# The emdawnwebgpu port (as shipped with emsdk 4.0.18) re-delivers async
# callbacks, which corrupts dlmalloc/emmalloc arenas a few frames later. The
# ASan allocator is immune to the duplicate frees and additionally reports
# skity-side heap overruns as case failures, so the harness is always built
# and run with AddressSanitizer.
target_compile_options(skity-web PRIVATE -fsanitize=address)

# Keep the build directory self-contained so the Python driver can serve one
# root: runner.html/runner.js land next to skity-web.js/.wasm. A separate
# target (instead of POST_BUILD) so editing the runner files re-copies them
# even when skity-web itself does not relink.
add_custom_target(skity-web-runner-files
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/runner.html
    ${CMAKE_CURRENT_SOURCE_DIR}/runner.js
    ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runner.html
          ${CMAKE_CURRENT_SOURCE_DIR}/runner.js
)
add_dependencies(skity-web skity-web-runner-files)

# skity's LINK_FLAGS property does not propagate to consumers, so the freetype
# port and the emdawnwebgpu glue must be re-added at link time.
target_link_options(skity-web PRIVATE
  "-fsanitize=address"
  "-sUSE_FREETYPE=1"
  "--use-port=emdawnwebgpu"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=SkityWebHarness"
  "-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString,HEAPU8"
  # The harness is a pure library: runner.js drives everything after module
  # load. Never auto-run main() and never let emscripten's maybeExit() tear
  # the runtime down after an async callback completes - later exported calls
  # would run on a dead heap.
  "-sINVOKE_RUN=0"
  "-sNO_EXIT_RUNTIME=1"
  "-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_smoke_init,_smoke_case_count,_smoke_case_name,_smoke_case_width,_smoke_case_height,_smoke_render_case,_smoke_readback_ready,_smoke_readback_data,_smoke_readback_size,_smoke_last_error,_smoke_case_cleanup"
  "-sENVIRONMENT=web"
)
