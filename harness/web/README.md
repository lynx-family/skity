# Web Harness (wasm geometry smoke test)

Renders a small set of geometry-only cases through the skity WebGPU backend
compiled to WebAssembly, inside a headless Chromium (SwiftShader WebGPU), and
compares the rendered pixels against checked-in baselines. This is the only
CI coverage that actually *runs* the wasm artifact - `wasm_build_check` only
verifies that it compiles.

The harness deliberately stays independent of `test/golden/` (which is bound
to Metal/macOS): cases, runner and baselines all live under `harness/web/`,
wired up the same way as `harness/font/` (`SKITY_ENABLE_WEB_HARNESS` CMake
option, `skity-web` target, `--suite=web-harness` in `tools/test-runner.py`).

## Layout

```
harness/web/
  cases.cc/.hpp     geometry case table (draw logic extracted from test/golden
                    cases; no text, no image files, no codec)
  main.cc           wasm entry; exports the smoke_* C API
  runner.html/.js   browser driver: preinitializes the WebGPU device, renders
                    every case offscreen, reads pixels back, hands them to
                    the Python driver as base64
  baselines/        checked-in expected PNGs (SwiftShader-WebGPU rendering)
```

Data flow: `runner.js` creates the WebGPU device and passes it to the module
(`Module.preinitializedWebGPUDevice` -> `emscripten_webgpu_get_device`), the
wasm side renders each case into an offscreen `WGPUTexture` via
`skity::WebContextCreate` + `GPUContext::CreateSurface` and copies the pixels
into a `MapRead` buffer. `tools/web_harness_runner.py` serves the build
directory over localhost, drives headless Chromium through Playwright,
compares the RGBA buffers against the baselines with the same tolerance
semantics as `test/golden/common/golden_test_check.cc`, and emits the
`<agent-test-report>` JSON (exit code 2 = infra error, 1 = pixel mismatch).

## Running locally

Requirements: emsdk 4.0.18 (`source emsdk_env.sh`), Python 3 with
`pip install playwright pillow` and `python -m playwright install chromium`.

```bash
# via the unified runner (configures + builds + runs)
python3 tools/test-runner.py --suite=web-harness --build-dir=out/cmake_wasm

# run only, on an existing build
python3 tools/test-runner.py --suite=web-harness --build-dir=out/cmake_wasm --run-only

# single case
python3 tools/test-runner.py --suite=web-harness --build-dir=out/cmake_wasm --run-only --filter=CatHeadPath
```

To create or refresh baselines (review the images before committing them):

```bash
SKITY_UPDATE_MISSING_GOLDEN=1 python3 tools/test-runner.py --suite=web-harness --build-dir=out/cmake_wasm --run-only
```

Baselines are captured from SwiftShader WebGPU. The Chromium/Playwright
version is pinned in CI; a Chromium update may require regenerating the
baselines (the comparison tolerances absorb small drift).

## Adding a case

1. Add an entry to the `kGeometryCases` table in `cases.cc` - copy the draw
   logic between `BeginRecording`/`FinishRecording` of the source golden case
   and keep the golden case name for traceability.
2. Rebuild and generate the baseline with `SKITY_UPDATE_MISSING_GOLDEN=1`,
   review the PNG, commit it.

## Implementation notes / known port quirks

- The harness runs on AddressSanitizer (`-fsanitize=address` on the
  `skity-web` target). The emdawnwebgpu port shipped with emsdk 4.0.18
  re-delivers async callbacks, which corrupts dlmalloc/emmalloc arenas a few
  frames later; the ASan allocator survives the duplicate frees and any
  skity-side heap overrun surfaces as a case failure.
- `wgpuBufferUnmap` is never called from the readback callback: in the port,
  unmap re-arms the pending-map abort path which re-delivers the callback on
  the same stack. The mapping memory is released by `wgpuBufferDestroy`
  during per-case cleanup instead.
- `mappedAtCreation` staging (the old upload path in
  `src/gpu/web/gpu_blit_pass_web.cc`) is replaced by `wgpuQueueWriteBuffer`
  uploads, partly because of the port's mapped-range bookkeeping.
- The module is a pure library: `-sINVOKE_RUN=0` + `-sNO_EXIT_RUNTIME=1`,
  otherwise emscripten tears the runtime down after the first async callback
  and every later exported call runs on a dead heap.
