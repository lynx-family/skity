// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

//
// End-to-end example of the skity C API consumed through the header-only
// C++ RAII layer (skity_hpp/skity.hpp).
//
// Same scene as case/c_api: a GLFW window with an OpenGL context, a static
// scene recorded once into a display list and replayed every frame, plus an
// orbiting dot redrawn per frame. The wrapper part shows the ownership
// model: Paint / Path / Typeface are owning RAII objects, Canvas is a
// borrowed view handed out by Surface::LockCanvas. Entry points the wrapper
// does not cover yet (shader creation, the recorder, display-list replay)
// are called through the raw C API — the intended mixed usage while the
// wrapper surface grows behind the coverage discipline.

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <skity_hpp/skity.hpp>

// The wrapper layer lives in skity::raii until libskity stops exporting the
// legacy C++ symbols (see the comment at the top of skity.hpp).
using namespace skity::raii;

// skity loads GL symbols at runtime via this callback (same contract as
// glfwGetProcAddress, just adapted to skity_gl_get_proc's return type).
static void* get_gl_proc(const char* name) {
  return (void*)glfwGetProcAddress(name);
}

int main() {
  if (glfwInit() == GLFW_FALSE) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(800, 600, "skity-capi-hpp", NULL, NULL);
  if (window == NULL) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  // -- skity GL context (owning wrapper) --
  Context context;
  if (Context::CreateGL(&get_gl_proc, &context) != SKITY_SUCCESS) {
    fprintf(stderr, "Context::CreateGL failed\n");
    return 1;
  }

  // -- surface bound to the default framebuffer (gl_id = 0) --
  // The create-info chain stays in C POD form; the wrapper only owns the
  // resulting handle.
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
  Surface surface;
  if (Surface::Create(context, surface_info, &surface) != SKITY_SUCCESS) {
    fprintf(stderr, "Surface::Create failed\n");
    return 1;
  }

  // -- shared drawing resources (owning wrappers) --
  Paint fill;
  fill.SetAntiAlias(true);

  // Not wrapped yet: create the shader through the C API and hand the raw
  // handle to Paint::SetShader, which takes a shared reference.
  skity_color4f grad_colors[] = {
      {1.f, 0.2f, 0.2f, 1.f}, {0.2f, 1.f, 0.4f, 1.f}, {0.3f, 0.4f, 1.f, 1.f}};
  float grad_pos[] = {0.f, 0.5f, 1.f};
  skity_point grad_pts[2] = {{200.f, 200.f, 0.f, 0.f},
                             {600.f, 520.f, 0.f, 0.f}};
  skity_shader shader = skity_shader_create_linear(
      grad_pts, grad_colors, grad_pos, 3, SKITY_TILE_MODE_CLAMP, 0);

  Paint grad_paint;
  grad_paint.SetAntiAlias(true);
  grad_paint.SetShader(shader);

  Path star;
  star.MoveTo(400.f, 150.f)
      .LineTo(470.f, 330.f)
      .LineTo(660.f, 330.f)
      .LineTo(510.f, 440.f)
      .LineTo(560.f, 620.f)
      .LineTo(400.f, 510.f)
      .LineTo(240.f, 620.f)
      .LineTo(290.f, 440.f)
      .LineTo(140.f, 330.f)
      .LineTo(330.f, 330.f)
      .Close();

  // -- record the static scene into a display list, once --
  // Not wrapped yet: drive the recorder through the C API, but note the
  // recording canvas is still usable through a Canvas view.
  skity_picture_recorder recorder = skity_picture_recorder_create();
  skity_rect cull = {0.f, 0.f, (float)winw, (float)winh};
  skity_picture_recorder_begin(recorder, &cull);
  skity_canvas rec_canvas = skity_picture_recorder_get_canvas(recorder);

  Rect bg_rect = Rect::MakeLTRB(80.f, 80.f, 320.f, 240.f);
  fill.SetColor(0xFF48DBFBu);
  Canvas(rec_canvas).DrawRect(bg_rect, fill);
  Canvas(rec_canvas).DrawPath(star, grad_paint);

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
    // Borrowed canvas view: one per frame, no cleanup needed.
    Canvas canvas = surface.LockCanvas();

    canvas.DrawColor(0xFF222222u, BlendMode::kSrc);

    // Replay the recorded static content (raw C: display list not wrapped).
    skity_display_list_draw(static_scene, canvas.get());

    // Dynamic content redrawn every frame: an orbiting dot.
    float t = (float)glfwGetTime();
    fill.SetColor(0xFFFFFFFFu);
    canvas.DrawCircle(400.f + 120.f * cosf(t), 330.f + 70.f * sinf(t * 1.3f),
                      28.f, fill);

    canvas.Flush();
    surface.Flush();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // -- teardown: wrappers release themselves; reverse order otherwise --
  skity_display_list_destroy(static_scene);
  skity_shader_destroy(shader);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
