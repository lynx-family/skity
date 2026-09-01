// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#include <GLFW/glfw3.h>
#include <skity_c/skity.h>
#include <skity_c/skity_context_vk.h>
#include <skity_c/skity_native_window_vk.h>
#include <volk.h>
#include <vulkan/vulkan.h>

#include <skity/macros.hpp>

#if defined(SKITY_MACOS)
#include <vulkan/vulkan_macos.h>
#include <vulkan/vulkan_metal.h>
#endif

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#if defined(SKITY_MACOS)
bool SetupCocoaVulkanWindow(GLFWwindow* window);
void* GetCocoaVulkanLayer(GLFWwindow* window);
void* GetCocoaVulkanView(GLFWwindow* window);
#elif defined(SKITY_WIN)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(SKITY_LINUX)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

namespace {

bool HasExtension(const char* const* names, uint32_t count,
                  const char* target) {
  for (uint32_t i = 0; i < count; ++i) {
    if (names[i] != nullptr && std::strcmp(names[i], target) == 0) {
      return true;
    }
  }
  return false;
}

bool BuildInstanceExtensions(GLFWwindow* window,
                             std::vector<const char*>* extensions) {
  uint32_t count = 0;
  const char** required = glfwGetRequiredInstanceExtensions(&count);
  if (required == nullptr || count == 0) {
    return false;
  }
  extensions->assign(required, required + count);

#if defined(SKITY_MACOS)
  if (!HasExtension(extensions->data(),
                    static_cast<uint32_t>(extensions->size()),
                    VK_EXT_METAL_SURFACE_EXTENSION_NAME)) {
    extensions->push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
  }
  if (!HasExtension(extensions->data(),
                    static_cast<uint32_t>(extensions->size()),
                    VK_MVK_MACOS_SURFACE_EXTENSION_NAME)) {
    extensions->push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
  }
#else
  (void)window;
#endif
  return true;
}

bool BuildNativeWindowInfo(GLFWwindow* window,
                           skity_vk_native_window_info* info) {
  *info = {};
#if defined(SKITY_WIN)
  info->type = SKITY_VK_NATIVE_WINDOW_TYPE_WIN32;
  info->handle = glfwGetWin32Window(window);
  info->secondary_handle = GetModuleHandle(nullptr);
  return info->handle != nullptr;
#elif defined(SKITY_MACOS)
  info->type = SKITY_VK_NATIVE_WINDOW_TYPE_METAL_LAYER;
  info->handle = GetCocoaVulkanLayer(window);
  info->secondary_handle = GetCocoaVulkanView(window);
  return info->handle != nullptr || info->secondary_handle != nullptr;
#elif defined(SKITY_LINUX)
  uint32_t count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&count);
  if (HasExtension(extensions, count, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME)) {
    info->type = SKITY_VK_NATIVE_WINDOW_TYPE_WAYLAND;
    info->handle = glfwGetWaylandDisplay();
    info->secondary_handle = glfwGetWaylandWindow(window);
    return info->handle != nullptr && info->secondary_handle != nullptr;
  }
  info->type = SKITY_VK_NATIVE_WINDOW_TYPE_XLIB;
  info->handle = glfwGetX11Display();
  info->window_id = static_cast<uint64_t>(glfwGetX11Window(window));
  return info->handle != nullptr && info->window_id != 0;
#else
  (void)window;
  return false;
#endif
}

float ContentScale(GLFWwindow* window) {
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  int window_width = 0;
  int window_height = 0;
  glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
  glfwGetWindowSize(window, &window_width, &window_height);
  if (framebuffer_width <= 0 || framebuffer_height <= 0 || window_width <= 0 ||
      window_height <= 0) {
    return 1.f;
  }
  return std::sqrt(static_cast<float>(framebuffer_width * framebuffer_width +
                                      framebuffer_height * framebuffer_height) /
                   static_cast<float>(window_width * window_width +
                                      window_height * window_height));
}

void DrawFrame(skity_canvas canvas, float time, skity_paint fill,
               skity_paint gradient, skity_path star) {
  skity_canvas_draw_color(canvas, 0xFF20232A, SKITY_BLEND_MODE_SRC);
  skity_canvas_draw_path(canvas, star, gradient);

  float radius = 24.f + 5.f * std::sin(time * 2.f);
  skity_canvas_draw_circle(canvas, 400.f + 110.f * std::cos(time),
                           330.f + 60.f * std::sin(time * 1.25f), radius, fill);
}

}  // namespace

int main() {
  if (volkInitialize() != VK_SUCCESS) {
    fprintf(stderr, "failed to load the Vulkan loader\n");
    return 1;
  }
  if (glfwInit() == GLFW_FALSE) {
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window =
      glfwCreateWindow(800, 600, "skity-capi-vk", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }
#if defined(SKITY_MACOS)
  if (!SetupCocoaVulkanWindow(window)) {
    fprintf(stderr, "failed to attach a CAMetalLayer\n");
    return 1;
  }
#endif

  std::vector<const char*> extensions;
  if (!BuildInstanceExtensions(window, &extensions)) {
    fprintf(stderr, "failed to query Vulkan instance extensions\n");
    return 1;
  }

  skity_context_create_info_vk context_info = {};
  context_info.get_instance_proc_addr = vkGetInstanceProcAddr;
  context_info.enabled_instance_extensions = extensions.data();
  context_info.enabled_instance_extension_count =
      static_cast<uint32_t>(extensions.size());
  context_info.enabled_instance_extensions_known = 1;

  skity_context context = nullptr;
  if (skity_context_create_vk_ex(&context_info, &context) != SKITY_SUCCESS) {
    fprintf(stderr, "skity_context_create_vk_ex failed\n");
    return 1;
  }

  skity_vk_native_window_info native_info = {};
  if (!BuildNativeWindowInfo(window, &native_info)) {
    fprintf(stderr, "unsupported Vulkan window platform\n");
    return 1;
  }
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
  skity_native_window_create_info_vk window_info = {};
  window_info.native_window = native_info;
  window_info.width = static_cast<uint32_t>(framebuffer_width);
  window_info.height = static_cast<uint32_t>(framebuffer_height);
  window_info.min_image_count = 2;
  window_info.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

  skity_native_window_vk native_window =
      skity_native_window_create_vk(context, &window_info);
  if (native_window == nullptr) {
    fprintf(stderr, "skity_native_window_create_vk failed\n");
    return 1;
  }

  skity_paint fill = skity_paint_create();
  skity_paint_set_anti_alias(fill, 1);
  skity_paint_set_color(fill, 0xFFFFFFFF);

  skity_color4f colors[] = {{1.f, 0.25f, 0.3f, 1.f}, {0.25f, 0.75f, 1.f, 1.f}};
  float positions[] = {0.f, 1.f};
  skity_point points[] = {{180.f, 180.f, 0.f, 0.f}, {620.f, 500.f, 0.f, 0.f}};
  skity_shader shader = skity_shader_create_linear(points, colors, positions, 2,
                                                   SKITY_TILE_MODE_CLAMP, 0);
  skity_paint gradient = skity_paint_create();
  skity_paint_set_anti_alias(gradient, 1);
  skity_paint_set_shader(gradient, shader);

  skity_path star = skity_path_create();
  skity_path_move_to(star, 400.f, 140.f);
  skity_path_line_to(star, 470.f, 320.f);
  skity_path_line_to(star, 660.f, 320.f);
  skity_path_line_to(star, 510.f, 430.f);
  skity_path_line_to(star, 560.f, 610.f);
  skity_path_line_to(star, 400.f, 500.f);
  skity_path_line_to(star, 240.f, 610.f);
  skity_path_line_to(star, 290.f, 430.f);
  skity_path_line_to(star, 140.f, 320.f);
  skity_path_line_to(star, 330.f, 320.f);
  skity_path_close(star);

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    skity_surface surface = nullptr;
    skity_result result = skity_native_window_acquire_next_surface_vk(
        native_window, 4, ContentScale(window), &surface);
    if (result == SKITY_ERROR_NEED_RECREATE) {
      glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
      skity_native_window_resize_vk(native_window,
                                    static_cast<uint32_t>(framebuffer_width),
                                    static_cast<uint32_t>(framebuffer_height));
      continue;
    }
    if (result != SKITY_SUCCESS) {
      break;
    }

    skity_canvas canvas = skity_surface_lock_canvas(surface, 1);
    if (canvas == nullptr) {
      skity_surface_destroy(surface);
      break;
    }
    DrawFrame(canvas, static_cast<float>(glfwGetTime()), fill, gradient, star);
    skity_canvas_flush(canvas);
    skity_canvas_destroy(canvas);

    result = skity_native_window_present_vk(native_window, &surface);
    if (result == SKITY_ERROR_NEED_RECREATE) {
      glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
      skity_native_window_resize_vk(native_window,
                                    static_cast<uint32_t>(framebuffer_width),
                                    static_cast<uint32_t>(framebuffer_height));
    } else if (result != SKITY_SUCCESS) {
      break;
    }
    glfwPollEvents();
  }

  skity_path_destroy(star);
  skity_paint_destroy(gradient);
  skity_shader_destroy(shader);
  skity_paint_destroy(fill);
  skity_native_window_destroy_vk(native_window);
  skity_context_destroy(context);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
