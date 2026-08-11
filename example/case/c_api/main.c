// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

//
// Minimal end-to-end example of the skity C API.
//
// Opens a GLFW window with an OpenGL context, records the static scene into a
// display list once (via skity_picture_recorder), then each frame replays it
// and draws a dynamic element on top — the classic display-list use case:
// record once, replay every frame.

#include <GLFW/glfw3.h>
#include <math.h>
#include <skity_c/skity.h>
#include <stdint.h>
#include <stdio.h>

// skity loads GL symbols at runtime via this callback (same contract as
// glfwGetProcAddress, just adapted to skity_gl_get_proc's return type).
static void* get_gl_proc(const char* name) {
  return (void*)glfwGetProcAddress(name);
}

int main(void) {
  if (glfwInit() == GLFW_FALSE) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(800, 600, "skity-capi", NULL, NULL);
  if (window == NULL) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  // -- skity GL context --
  skity_context ctx;
  if (skity_context_create_gl(get_gl_proc, &ctx) != SKITY_SUCCESS) {
    fprintf(stderr, "skity_context_create_gl failed\n");
    return 1;
  }

  // -- surface bound to the default framebuffer (gl_id = 0) --
  int fbw = 0, fbh = 0, winw = 0, winh = 0;
  glfwGetFramebufferSize(window, &fbw, &fbh);
  glfwGetWindowSize(window, &winw, &winh);
  float scale = sqrtf((float)(fbw * fbw + fbh * fbh) /
                      (float)(winw * winw + winh * winh));

  skity_surface_create_info_gl gl_ext = {
      .s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL,
      .surface_type = SKITY_GL_SURFACE_TYPE_FRAMEBUFFER,
      .gl_id = 0,
      .has_stencil = 1,
  };
  skity_surface_create_info surface_info = {
      .s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO,
      .p_next = &gl_ext,
      .width = (uint32_t)winw,
      .height = (uint32_t)winh,
      .sample_count = 4,
      .content_scale = scale,
  };
  skity_surface surface;
  if (skity_surface_create(ctx, &surface_info, &surface) != SKITY_SUCCESS) {
    fprintf(stderr, "skity_surface_create failed\n");
    return 1;
  }

  // -- shared drawing resources --
  skity_paint fill = skity_paint_create();
  skity_paint_set_anti_alias(fill, 1);

  skity_color4f grad_colors[] = {
      {1.f, 0.2f, 0.2f, 1.f}, {0.2f, 1.f, 0.4f, 1.f}, {0.3f, 0.4f, 1.f, 1.f}};
  float grad_pos[] = {0.f, 0.5f, 1.f};
  skity_point grad_pts[2] = {{200.f, 200.f, 0.f, 0.f},
                             {600.f, 520.f, 0.f, 0.f}};
  skity_shader shader = skity_shader_create_linear(
      grad_pts, grad_colors, grad_pos, 3, SKITY_TILE_MODE_CLAMP, 0);

  skity_paint grad_paint = skity_paint_create();
  skity_paint_set_anti_alias(grad_paint, 1);
  skity_paint_set_shader(grad_paint, shader);

  skity_path star = skity_path_create();
  skity_path_move_to(star, 400.f, 150.f);
  skity_path_line_to(star, 470.f, 330.f);
  skity_path_line_to(star, 660.f, 330.f);
  skity_path_line_to(star, 510.f, 440.f);
  skity_path_line_to(star, 560.f, 620.f);
  skity_path_line_to(star, 400.f, 510.f);
  skity_path_line_to(star, 240.f, 620.f);
  skity_path_line_to(star, 290.f, 440.f);
  skity_path_line_to(star, 140.f, 330.f);
  skity_path_line_to(star, 330.f, 330.f);
  skity_path_close(star);

  // -- record the static scene into a display list, once --
  // The recording canvas is a regular skity_canvas, so any skity_canvas_*
  // call records into the list instead of drawing immediately.
  skity_picture_recorder recorder = skity_picture_recorder_create();
  skity_rect cull = {0.f, 0.f, (float)winw, (float)winh};
  skity_picture_recorder_begin(recorder, &cull);
  skity_canvas rec_canvas = skity_picture_recorder_get_canvas(recorder);

  skity_rect bg_rect = {80.f, 80.f, 320.f, 240.f};
  skity_paint_set_color(fill, 0xFF48DBFBu);
  skity_canvas_draw_rect(rec_canvas, &bg_rect, fill);
  skity_canvas_draw_path(rec_canvas, star, grad_paint);

  skity_display_list static_scene;
  if (skity_picture_recorder_finish(recorder, &static_scene) != SKITY_SUCCESS) {
    fprintf(stderr, "skity_picture_recorder_finish failed\n");
    return 1;
  }
  // The display list is independent of the recorder now; the recorder and the
  // recording-canvas handle are both invalid after finish.
  skity_picture_recorder_destroy(recorder);

  // -- render loop --
  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    skity_canvas canvas = skity_surface_lock_canvas(surface, 1);

    skity_canvas_draw_color(canvas, 0xFF222222u, SKITY_BLEND_MODE_SRC);

    // Replay the recorded static content.
    skity_display_list_draw(static_scene, canvas);

    // Dynamic content redrawn every frame: an orbiting dot.
    float t = (float)glfwGetTime();
    skity_paint_set_color(fill, 0xFFFFFFFFu);
    skity_canvas_draw_circle(canvas, 400.f + 120.f * cosf(t),
                             330.f + 70.f * sinf(t * 1.3f), 28.f, fill);

    skity_canvas_flush(canvas);
    skity_surface_flush(surface);
    skity_canvas_destroy(canvas);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // -- teardown (reverse order of creation) --
  skity_display_list_destroy(static_scene);
  skity_path_destroy(star);
  skity_paint_destroy(grad_paint);
  skity_shader_destroy(shader);
  skity_paint_destroy(fill);
  skity_surface_destroy(surface);
  skity_context_destroy(ctx);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
