// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// A pure-C demo of the serialization surface: record with the C recorder,
// capture a display list, wrap it into a picture, write it out as .skp.

#include <skity/c/skity_canvas.h>
#include <skity/c/skity_paint.h>
#include <skity/c/skity_picture.h>
#include <skity/c/skity_recorder.h>
#include <skity/c/skity_shader.h>
#include <skity/c/skity_stream.h>

int main(void) {
  skity_picture_recorder recorder = skity_picture_recorder_create();
  skity_picture_recorder_begin(recorder, nullptr);

  skity_canvas canvas = skity_picture_recorder_get_canvas(recorder);

  skity_paint paint = skity_paint_create();

  skity_color4f colors[3] = {
      {{1.f, 0.f, 0.f, 1.f}},
      {{0.f, 1.f, 0.f, 1.f}},
      {{0.f, 0.f, 1.f, 1.f}},
  };
  float offset[3] = {0.f, 0.5f, 1.f};
  skity_point center = {{50.f, 50.f, 0.f, 1.f}};

  skity_shader shader = skity_shader_create_radial(center, 50.f, colors, offset,
                                                   3, SKITY_TILE_MODE_CLAMP, 0);
  skity_paint_set_shader(paint, shader);
  skity_shader_destroy(shader);

  skity_rect rect = {0.f, 0.f, 100.f, 100.f};
  skity_canvas_draw_rect(canvas, &rect, paint);
  skity_paint_destroy(paint);

  skity_display_list display_list = nullptr;
  skity_picture_recorder_finish(recorder, &display_list);
  skity_picture_recorder_destroy(recorder);

  skity_picture picture = skity_picture_create_from_display_list(display_list);
  skity_display_list_destroy(display_list);

  skity_write_stream stream =
      skity_write_stream_create_from_file("linear-gradient.skp");
  skity_picture_serialize(picture, stream, nullptr);
  skity_write_stream_flush(stream);

  skity_write_stream_destroy(stream);
  skity_picture_destroy(picture);

  return 0;
}
