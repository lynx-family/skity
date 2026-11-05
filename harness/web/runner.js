// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Browser-side driver for the skity web harness.
//
// Flow: preinitialize a WebGPU device on the Emscripten Module so the wasm
// side can pick it up with emscripten_webgpu_get_device(), render every
// geometry case offscreen, read the pixels back through the MapRead buffer
// and expose the results (RGBA as base64) on window for the Playwright
// driver running in Python.

'use strict';

function harnessFatal(message) {
  window.__harnessFatal = String(message);
  window.__harnessDone = true;
}

function bytesToBase64(bytes) {
  let binary = '';
  const chunkSize = 0x8000;
  for (let i = 0; i < bytes.length; i += chunkSize) {
    binary += String.fromCharCode.apply(null, bytes.subarray(i, i + chunkSize));
  }
  return btoa(binary);
}

async function runCase(module, index) {
  const entry = {
    name: module.UTF8ToString(module._smoke_case_name(index)),
    width: module._smoke_case_width(index),
    height: module._smoke_case_height(index),
    ok: false,
    nonEmptyPixels: 0,
    pixels: null,
    error: null,
  };
  try {
    if (module._smoke_render_case(index) !== 0) {
      throw new Error(module.UTF8ToString(module._smoke_last_error()));
    }
    // The mapAsync callback only fires on event-loop turns, so poll gently.
    for (;;) {
      const status = module._smoke_readback_ready();
      if (status === -1) {
        throw new Error(module.UTF8ToString(module._smoke_last_error()));
      }
      if (status === 1) {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 5));
    }
    const ptr = module._smoke_readback_data();
    const size = module._smoke_readback_size();
    const bytes = module.HEAPU8.slice(ptr, ptr + size);
    let nonEmpty = 0;
    for (let p = 3; p < size; p += 4) {
      if (bytes[p] !== 0) {
        nonEmpty++;
      }
    }
    entry.nonEmptyPixels = nonEmpty;
    entry.pixels = bytesToBase64(bytes);
    // A fully transparent frame means the render or readback silently failed.
    entry.ok = nonEmpty > 0;
    if (!entry.ok) {
      entry.error = 'all pixels are transparent';
    }
  } catch (e) {
    entry.error = String((e && e.message) || e);
  } finally {
    module._smoke_case_cleanup();
  }
  return entry;
}

async function runHarness() {
  if (!navigator.gpu) {
    harnessFatal('navigator.gpu is not available');
    return;
  }
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) {
    harnessFatal('WebGPU adapter unavailable (SwiftShader disabled?)');
    return;
  }
  const device = await adapter.requestDevice();
  // noExitRuntime: after the async map callback completes, emscripten's
  // maybeExit() would otherwise tear the runtime down implicitly (atexit +
  // teardown), leaving every later exported call running on a dead heap.
  const module = await SkityWebHarness({
    preinitializedWebGPUDevice: device,
    noExitRuntime: true,
  });

  if (module._smoke_init() !== 0) {
    harnessFatal(
        'smoke_init failed: ' + module.UTF8ToString(module._smoke_last_error()));
    return;
  }

  const results = [];
  const count = module._smoke_case_count();
  for (let i = 0; i < count; i++) {
    results.push(await runCase(module, i));
  }
  window.__harnessResults = results;
  window.__harnessDone = true;
}

window.addEventListener('load', () => {
  runHarness().catch((e) => harnessFatal('unexpected: ' + String(e)));
});
