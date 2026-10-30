// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity.h>

int skity_test_path_builder_c_smoke(void) {
  int ok = 0;
  skity_path_builder builder = skity_path_builder_create();
  skity_path path = skity_path_create();
  skity_paint paint = skity_paint_create();
  skity_picture_recorder recorder = skity_picture_recorder_create();
  skity_canvas canvas = NULL;
  skity_display_list list = NULL;
  if (!builder || !path || !paint || !recorder) goto cleanup;

  skity_path_builder_reserve(builder, 64, 64, 8);
  skity_path_builder_move_to(builder, 10, 10);
  skity_path_builder_line_to(builder, 90, 10);
  skity_path_builder_quad_to(builder, 90, 50, 10, 90);
  skity_path_builder_close(builder);
  if (!skity_path_builder_detach_to(builder, path)) goto cleanup;
  if (!skity_path_builder_is_empty(builder)) goto cleanup;

  skity_rect bounds = {0, 0, 100, 100};
  skity_picture_recorder_begin(recorder, &bounds);
  canvas = skity_picture_recorder_get_canvas(recorder);
  if (!canvas) goto cleanup;
  skity_canvas_draw_path(canvas, path, paint);
  skity_path_builder_add_circle(builder, 50, 50, 20, SKITY_PATH_DIRECTION_CW);
  if (!skity_path_builder_detach_to(builder, path)) goto cleanup;
  skity_canvas_draw_path(canvas, path, paint);
  skity_path_reset(path);
  if (skity_picture_recorder_finish(recorder, &list) != SKITY_SUCCESS)
    goto cleanup;
  ok = skity_display_list_get_op_count(list) == 2;

cleanup:
  skity_canvas_destroy(canvas);
  skity_display_list_destroy(list);
  skity_picture_recorder_destroy(recorder);
  skity_paint_destroy(paint);
  skity_path_destroy(path);
  skity_path_builder_destroy(builder);
  return ok ? 0 : 1;
}
