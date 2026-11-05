// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Web harness entry for the wasm geometry smoke tests.
//
// The harness is compiled with emcc and loaded by harness/web/runner.html.
// The browser side creates the WebGPU device
// (Module.preinitializedWebGPUDevice) and drives the flow; the exported C
// interface below renders each geometry case into an offscreen WGPUTexture and
// copies the pixels back into a MapRead buffer. Pixels are handed to JS through
// the wasm heap, so the only values crossing the JS/wasm boundary are integers
// and raw memory.

#include <webgpu/webgpu.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_context_web.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <string>

#include "harness/web/cases.hpp"

// Only the harness objects are ASan-instrumented (the skity library is not),
// which makes libc++ container annotations report false container-overflows
// on vectors built by the non-instrumented side. The annotations are
// disabled; heap-use-after-free / double-free detection - the reason the
// harness runs on ASan - stays on.
extern "C" const char* __asan_default_options() {
  return "detect_container_overflow=0";
}

namespace {

using skity::web_harness::GeometryCase;

skity::GPUContext* g_context = nullptr;
WGPUDevice g_device = nullptr;
WGPUQueue g_queue = nullptr;

// Per-case WebGPU resources, kept alive until JS picked up the pixels.
WGPUTexture g_texture = nullptr;
WGPUBuffer g_readback_buffer = nullptr;
std::unique_ptr<skity::GPUSurface> g_surface;

// Readback state machine: 0 = pending, 1 = pixels ready, -1 = failed.
int32_t g_readback_status = 0;
uint8_t* g_pixels = nullptr;
uint32_t g_pixels_size = 0;
// Fixed-size buffer: cpplint forbids global std::string, and the harness only
// needs short diagnostic messages.
char g_last_error[256] = {0};

void SetError(const std::string& message) {
  std::snprintf(g_last_error, sizeof(g_last_error), "%s", message.c_str());
  g_readback_status = -1;
}

void ReleaseCaseResources() {
  g_surface.reset();
  if (g_readback_buffer != nullptr) {
    // Destroy runs the pending on-unmap callbacks (freeing the mapping the
    // port allocated in GetConstMappedRange); Release drops the object.
    wgpuBufferDestroy(g_readback_buffer);
    wgpuBufferRelease(g_readback_buffer);
    g_readback_buffer = nullptr;
  }
  if (g_texture != nullptr) {
    wgpuTextureRelease(g_texture);
    g_texture = nullptr;
  }
  delete[] g_pixels;
  g_pixels = nullptr;
  g_pixels_size = 0;
}

void OnReadbackMapped(WGPUMapAsyncStatus status, WGPUStringView message,
                      void* userdata1, void* userdata2) {
  // The emdawnwebgpu port can deliver this map callback a second time while
  // the first invocation is still inside wgpuBufferUnmap() (the unmap
  // synchronously re-enters the event dispatcher). A second pass would
  // memalign the very address the first Unmap just freed and free it again
  // (double free, surfacing as heap corruption frames later), so guard on
  // the readback state.
  if (g_readback_status != 0) {
    return;
  }
  if (status != WGPUMapAsyncStatus_Success) {
    SetError("readback buffer map failed, status=" +
             std::to_string(static_cast<int>(status)));
    return;
  }

  const uint32_t case_width = (uint32_t)(uintptr_t)userdata1;
  const uint32_t case_height = (uint32_t)(uintptr_t)userdata2;
  const uint32_t bytes_per_row = (case_width * 4u + 255u) & ~255u;
  // Read-mode mappings must go through the const variant; the mutable one
  // asserts unless the buffer was mapped for writing.
  const void* mapped = wgpuBufferGetConstMappedRange(
      g_readback_buffer, 0, bytes_per_row * case_height);
  if (mapped == nullptr) {
    SetError("wgpuBufferGetConstMappedRange returned null");
    return;
  }

  // Drop the 256-byte row padding so JS receives a tight RGBA buffer.
  g_pixels_size = case_width * case_height * 4u;
  g_pixels = new uint8_t[g_pixels_size];
  const uint8_t* src = static_cast<const uint8_t*>(mapped);
  for (uint32_t row = 0; row < case_height; row++) {
    std::copy(src + row * bytes_per_row,
              src + row * bytes_per_row + case_width * 4u,
              g_pixels + row * case_width * 4u);
  }
  // Publish the result and leave the buffer mapped. Do NOT call
  // wgpuBufferUnmap here: in the emdawnwebgpu port, Unmap re-arms the
  // pending-map abort path which re-delivers this callback (and frees the
  // mapping) while the first invocation is still on the stack. The mapping
  // memory is released exactly once by wgpuBufferDestroy in
  // ReleaseCaseResources instead.
  g_readback_status = 1;
}

}  // namespace

extern "C" {

/**
 * Acquire the WebGPU device preinitialized by the browser side
 * (Module.preinitializedWebGPUDevice) and create the skity GPUContext.
 */
int32_t smoke_init() {
  if (g_context != nullptr) {
    return 0;
  }
  g_device = emscripten_webgpu_get_device();
  if (g_device == nullptr) {
    SetError(
        "emscripten_webgpu_get_device returned null; did runner.js set "
        "Module.preinitializedWebGPUDevice?");
    printf("[web-harness] init failed: %s\n", g_last_error);
    return -1;
  }
  g_queue = wgpuDeviceGetQueue(g_device);
  g_context = skity::WebContextCreate(g_device, g_queue).release();
  if (g_context == nullptr) {
    SetError("skity::WebContextCreate failed");
    return -1;
  }
  return 0;
}

uint32_t smoke_case_count() { return skity::web_harness::GetCaseCount(); }

const char* smoke_case_name(uint32_t index) {
  if (index >= smoke_case_count()) {
    return nullptr;
  }
  return skity::web_harness::GetGeometryCases()[index].name;
}

uint32_t smoke_case_width(uint32_t index) {
  return skity::web_harness::GetGeometryCases()[index].width;
}

uint32_t smoke_case_height(uint32_t index) {
  return skity::web_harness::GetGeometryCases()[index].height;
}

/**
 * Render case `index` into a fresh offscreen WGPUTexture and start the async
 * readback. Poll smoke_readback_ready() afterwards.
 */
int32_t smoke_render_case(uint32_t index) {
  if (g_context == nullptr) {
    SetError("smoke_init was not called or failed");
    return -1;
  }
  if (index >= smoke_case_count()) {
    SetError("case index out of range: " + std::to_string(index));
    return -1;
  }

  ReleaseCaseResources();
  g_readback_status = 0;
  g_last_error[0] = '\0';

  const GeometryCase& test_case = skity::web_harness::GetGeometryCases()[index];

  WGPUTextureDescriptor texture_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
  texture_desc.usage =
      WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
  texture_desc.dimension = WGPUTextureDimension_2D;
  texture_desc.size = {test_case.width, test_case.height, 1};
  texture_desc.format = WGPUTextureFormat_RGBA8Unorm;
  texture_desc.mipLevelCount = 1;
  texture_desc.sampleCount = 1;
  g_texture = wgpuDeviceCreateTexture(g_device, &texture_desc);
  if (g_texture == nullptr) {
    SetError("wgpuDeviceCreateTexture failed");
    return -1;
  }

  skity::GPUSurfaceDescriptorWEB surface_desc = {};
  surface_desc.backend = skity::GPUBackendType::kWebGPU;
  surface_desc.width = test_case.width;
  surface_desc.height = test_case.height;
  // The golden env defaults to sample_count 4, but the WebGPU surface is a
  // single-sample resolve-less target; the smoke baselines are captured with
  // sample_count 1.
  surface_desc.sample_count = 1;
  surface_desc.texture = g_texture;
  g_surface = g_context->CreateSurface(&surface_desc);
  if (g_surface == nullptr) {
    SetError("GPUContext::CreateSurface failed");
    ReleaseCaseResources();
    return -1;
  }

  skity::Canvas* canvas = g_surface->LockCanvas();
  test_case.render(canvas);
  canvas->Flush();
  g_surface->Flush();

  const uint32_t bytes_per_row =
      (test_case.width * 4u + 255u) & ~255u;  // WebGPU requires 256 alignment
  WGPUBufferDescriptor buffer_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
  buffer_desc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
  buffer_desc.size = bytes_per_row * test_case.height;
  g_readback_buffer = wgpuDeviceCreateBuffer(g_device, &buffer_desc);
  if (g_readback_buffer == nullptr) {
    SetError("wgpuDeviceCreateBuffer failed");
    ReleaseCaseResources();
    return -1;
  }

  WGPUCommandEncoderDescriptor encoder_desc =
      WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
  WGPUCommandEncoder encoder =
      wgpuDeviceCreateCommandEncoder(g_device, &encoder_desc);

  WGPUTexelCopyTextureInfo copy_src = {};
  copy_src.texture = g_texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};
  copy_src.aspect = WGPUTextureAspect_All;

  WGPUTexelCopyBufferLayout copy_layout = {};
  copy_layout.offset = 0;
  copy_layout.bytesPerRow = bytes_per_row;
  copy_layout.rowsPerImage = test_case.height;

  WGPUTexelCopyBufferInfo copy_dst = {};
  copy_dst.layout = copy_layout;
  copy_dst.buffer = g_readback_buffer;

  WGPUExtent3D copy_size = {test_case.width, test_case.height, 1};
  wgpuCommandEncoderCopyTextureToBuffer(encoder, &copy_src, &copy_dst,
                                        &copy_size);

  WGPUCommandBufferDescriptor cmd_desc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
  WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_desc);
  wgpuQueueSubmit(g_queue, 1, &cmd_buffer);
  wgpuCommandBufferRelease(cmd_buffer);
  wgpuCommandEncoderRelease(encoder);

  WGPUBufferMapCallbackInfo map_callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
  // AllowSpontaneous: the callback fires by itself on browser event-loop
  // turns. Nobody calls wgpuInstanceProcessEvents here, so the other modes
  // would never deliver the callback to the JS-side polling in runner.js.
  map_callback.mode = WGPUCallbackMode_AllowSpontaneous;
  map_callback.callback = &OnReadbackMapped;
  map_callback.userdata1 =
      (void*)(uintptr_t)test_case.width;  // NOLINT(readability/casting)
  map_callback.userdata2 =
      (void*)(uintptr_t)test_case.height;  // NOLINT(readability/casting)
  wgpuBufferMapAsync(g_readback_buffer, WGPUMapMode_Read, 0, buffer_desc.size,
                     map_callback);
  return 0;
}

/** 0 = pending, 1 = pixels ready, -1 = failed (see smoke_last_error). */
int32_t smoke_readback_ready() { return g_readback_status; }

/** Tight RGBA8888 pixel data, valid while status == 1 and before cleanup. */
uint8_t* smoke_readback_data() { return g_pixels; }

uint32_t smoke_readback_size() { return g_pixels_size; }

const char* smoke_last_error() { return g_last_error; }

/** Release per-case GPU resources. Call after JS picked up the pixels. */
void smoke_case_cleanup() { ReleaseCaseResources(); }

}  // extern "C"

int main() {
  // Nothing to do at startup: runner.js calls smoke_init() after the module
  // runtime is ready and the WebGPU device has been preinitialized.
  return 0;
}
