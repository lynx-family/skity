// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Unit tests for the C ABI entries backing the codec / io migration off the
// legacy C++ API: flat path data exchange, text-blob run extraction, and the
// delegate canvas that observes a display-list op stream through callbacks.

#include <gtest/gtest.h>
#include <skity/c/skity_canvas.h>
#include <skity/c/skity_font.h>
#include <skity/c/skity_paint.h>
#include <skity/c/skity_path.h>
#include <skity/c/skity_recorder.h>
#include <skity/c/skity_text.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct PathGuard {
  skity_path p = skity_path_create();
  ~PathGuard() { skity_path_destroy(p); }
};

struct PaintGuard {
  skity_paint p = skity_paint_create();
  ~PaintGuard() { skity_paint_destroy(p); }
};

struct FontGuard {
  skity_font f = nullptr;
  ~FontGuard() { skity_font_destroy(f); }
};

struct TypefaceGuard {
  skity_typeface t = nullptr;
  ~TypefaceGuard() { skity_typeface_destroy(t); }
};

struct BlobGuard {
  skity_text_blob b = nullptr;
  ~BlobGuard() { skity_text_blob_destroy(b); }
};

struct CanvasGuard {
  skity_canvas c = nullptr;
  ~CanvasGuard() { skity_canvas_destroy(c); }
};

struct DisplayListGuard {
  skity_display_list l = nullptr;
  ~DisplayListGuard() { skity_display_list_destroy(l); }
  skity_display_list release() {
    skity_display_list out = l;
    l = nullptr;
    return out;
  }
};

// Accumulates the op stream a delegate canvas delivers, both as event names
// and as the payloads needed by the assertions below.
struct DelegateSink {
  std::vector<std::string> events;
  float last_translate[2] = {-1.f, -1.f};
  int last_clip_op = -1;
  int last_path_verb_count = -1;
  int destroyed = 0;

  void Record(const char* name) { events.emplace_back(name); }
};

void SinkTranslate(void* userdata, float dx, float dy) {
  auto* sink = static_cast<DelegateSink*>(userdata);
  sink->Record("translate");
  sink->last_translate[0] = dx;
  sink->last_translate[1] = dy;
}

void SinkSave(void* userdata) {
  static_cast<DelegateSink*>(userdata)->Record("save");
}

void SinkRestore(void* userdata) {
  static_cast<DelegateSink*>(userdata)->Record("restore");
}

void SinkClipRect(void* userdata, const skity_rect*, skity_clip_op op) {
  auto* sink = static_cast<DelegateSink*>(userdata);
  sink->Record("clip_rect");
  sink->last_clip_op = static_cast<int>(op);
}

void SinkDrawRect(void* userdata, const skity_rect*, skity_paint) {
  static_cast<DelegateSink*>(userdata)->Record("draw_rect");
}

// The path handle is borrowed; reading it inside the callback is exactly the
// usage the Phase-3 recording implementation will make of it.
void SinkDrawPath(void* userdata, skity_path path, skity_paint) {
  auto* sink = static_cast<DelegateSink*>(userdata);
  sink->Record("draw_path");
  sink->last_path_verb_count = skity_path_count_verbs(path);
}

void SinkDrawBlob(void* userdata, skity_text_blob blob, float, float,
                  skity_paint) {
  auto* sink = static_cast<DelegateSink*>(userdata);
  sink->Record("draw_blob");
  // The borrowed blob stays queryable during the callback.
  sink->last_path_verb_count = skity_text_blob_get_run_count(blob);
}

void SinkDestroy(void* userdata) {
  static_cast<DelegateSink*>(userdata)->destroyed += 1;
}

skity_canvas_delegate_callbacks SinkCallbacks() {
  skity_canvas_delegate_callbacks cb{};
  cb.on_translate = &SinkTranslate;
  cb.on_save = &SinkSave;
  cb.on_restore = &SinkRestore;
  cb.on_clip_rect = &SinkClipRect;
  cb.on_draw_rect = &SinkDrawRect;
  cb.on_draw_path = &SinkDrawPath;
  cb.on_draw_blob = &SinkDrawBlob;
  cb.on_destroy = &SinkDestroy;
  return cb;
}

}  // namespace

TEST(CapiSerialization, PathCloneSurvivesSourceMutationAndDestruction) {
  PathGuard clone;
  {
    PathGuard source;
    skity_path_move_to(source.p, 1, 2);
    skity_path_line_to(source.p, 3, 4);
    skity_path_destroy(clone.p);
    clone.p = skity_path_clone(source.p);
    skity_path_set_last_pt(source.p, 30, 40);
    skity_path_line_to(clone.p, 5, 6);
    EXPECT_EQ(skity_path_count_points(source.p), 2u);
  }
  skity_point points[3];
  ASSERT_EQ(skity_path_get_points(clone.p, points, 3), 3);
  EXPECT_FLOAT_EQ(points[1].x, 3);
  EXPECT_FLOAT_EQ(points[1].y, 4);
  EXPECT_FLOAT_EQ(points[2].x, 5);
}

TEST(CapiSerialization, PathFlatDataExchange) {
  PathGuard path;
  skity_path_move_to(path.p, 10.f, 20.f);
  skity_path_line_to(path.p, 30.f, 40.f);
  skity_path_quad_to(path.p, 50.f, 60.f, 70.f, 80.f);
  skity_path_conic_to(path.p, 90.f, 100.f, 110.f, 120.f, 0.5f);
  skity_path_cubic_to(path.p, 130.f, 140.f, 150.f, 160.f, 170.f, 180.f);
  skity_path_close(path.p);
  skity_path_set_fill_type(path.p, SKITY_PATH_FILL_TYPE_EVEN_ODD);

  EXPECT_EQ(skity_path_get_fill_type(path.p), SKITY_PATH_FILL_TYPE_EVEN_ODD);

  // Count-only queries answer without touching any buffer.
  ASSERT_EQ(skity_path_get_verbs(path.p, nullptr, 0), 6);
  ASSERT_EQ(skity_path_get_points(path.p, nullptr, 0), 9);
  ASSERT_EQ(skity_path_get_conic_weights(path.p, nullptr, 0), 1);

  skity_path_verb verbs[8];
  ASSERT_EQ(skity_path_get_verbs(path.p, verbs, 8), 6);
  const skity_path_verb expected_verbs[] = {
      SKITY_PATH_VERB_MOVE,  SKITY_PATH_VERB_LINE,  SKITY_PATH_VERB_QUAD,
      SKITY_PATH_VERB_CONIC, SKITY_PATH_VERB_CUBIC, SKITY_PATH_VERB_CLOSE};
  EXPECT_EQ(std::memcmp(verbs, expected_verbs, sizeof(expected_verbs)), 0);

  skity_point points[12];
  ASSERT_EQ(skity_path_get_points(path.p, points, 12), 9);
  EXPECT_FLOAT_EQ(points[0].x, 10.f);
  EXPECT_FLOAT_EQ(points[0].y, 20.f);
  EXPECT_FLOAT_EQ(points[1].x, 30.f);
  EXPECT_FLOAT_EQ(points[8].x, 170.f);
  EXPECT_FLOAT_EQ(points[8].y, 180.f);

  float weight = -1.f;
  ASSERT_EQ(skity_path_get_conic_weights(path.p, &weight, 1), 1);
  EXPECT_FLOAT_EQ(weight, 0.5f);

  // Too-small buffers fill what fits and report how much was written.
  std::fill(std::begin(verbs), std::end(verbs), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_get_verbs(path.p, verbs, 3), 3);
  EXPECT_EQ(verbs[0], SKITY_PATH_VERB_MOVE);
  EXPECT_EQ(verbs[2], SKITY_PATH_VERB_QUAD);
  EXPECT_EQ(verbs[3], SKITY_PATH_VERB_DONE);

  // A path without conics reports an empty weight array.
  PathGuard lines_only;
  skity_path_move_to(lines_only.p, 0.f, 0.f);
  skity_path_line_to(lines_only.p, 1.f, 1.f);
  EXPECT_EQ(skity_path_get_conic_weights(lines_only.p, nullptr, 0), 0);

  // Empty / invalid paths answer zero everywhere.
  PathGuard empty;
  EXPECT_EQ(skity_path_get_verbs(empty.p, nullptr, 0), 0);
  EXPECT_EQ(skity_path_get_points(empty.p, nullptr, 0), 0);
  EXPECT_EQ(skity_path_get_conic_weights(empty.p, nullptr, 0), 0);
  EXPECT_EQ(skity_path_get_verbs(nullptr, nullptr, 0), 0);
}

TEST(CapiSerialization, TextBlobRunExtraction) {
  TypefaceGuard typeface{skity_typeface_get_default()};
  if (typeface.t == nullptr) {
    GTEST_SKIP() << "default typeface unavailable";
  }
  FontGuard font{skity_font_create_with_typeface(typeface.t, 24.f)};
  ASSERT_NE(font.f, nullptr);

  const uint16_t glyphs[3] = {10, 20, 30};
  const float pos_x[3] = {1.f, 2.f, 3.f};
  const float pos_y[3] = {4.f, 5.f, 6.f};

  BlobGuard blob{
      skity_text_blob_create_from_glyphs(font.f, glyphs, pos_x, pos_y, 3)};
  ASSERT_NE(blob.b, nullptr);

  ASSERT_EQ(skity_text_blob_get_run_count(blob.b), 1);
  ASSERT_EQ(skity_text_blob_get_run_count(nullptr), 0);

  ASSERT_EQ(skity_text_blob_get_run_glyph_count(blob.b, 0), 3);
  uint16_t out_glyphs[3] = {};
  ASSERT_EQ(skity_text_blob_get_run_glyphs(blob.b, 0, out_glyphs, 3), 3);
  EXPECT_EQ(std::memcmp(out_glyphs, glyphs, sizeof(glyphs)), 0);

  float out_x[3] = {};
  ASSERT_EQ(skity_text_blob_get_run_pos_x(blob.b, 0, out_x, 3), 3);
  EXPECT_FLOAT_EQ(out_x[2], 3.f);

  float out_y[3] = {};
  ASSERT_EQ(skity_text_blob_get_run_pos_y(blob.b, 0, out_y, 3), 3);
  EXPECT_FLOAT_EQ(out_y[0], 4.f);

  FontGuard run_font{skity_text_blob_get_run_font(blob.b, 0)};
  ASSERT_NE(run_font.f, nullptr);
  EXPECT_FLOAT_EQ(skity_font_get_size(run_font.f), 24.f);
  TypefaceGuard run_typeface{skity_font_get_typeface(run_font.f)};
  ASSERT_NE(run_typeface.t, nullptr);
  EXPECT_EQ(skity_typeface_get_unique_id(run_typeface.t),
            skity_typeface_get_unique_id(typeface.t));

  // Horizontal-only positioning leaves the y array empty.
  BlobGuard x_only{
      skity_text_blob_create_from_glyphs(font.f, glyphs, pos_x, nullptr, 3)};
  ASSERT_NE(x_only.b, nullptr);
  EXPECT_EQ(skity_text_blob_get_run_pos_x(x_only.b, 0, nullptr, 0), 3);
  EXPECT_EQ(skity_text_blob_get_run_pos_y(x_only.b, 0, nullptr, 0), 0);

  // Natural-advance runs carry no positions at all.
  BlobGuard natural{
      skity_text_blob_create_from_glyphs(font.f, glyphs, nullptr, nullptr, 3)};
  ASSERT_NE(natural.b, nullptr);
  EXPECT_EQ(skity_text_blob_get_run_pos_x(natural.b, 0, nullptr, 0), 0);
  EXPECT_EQ(skity_text_blob_get_run_pos_y(natural.b, 0, nullptr, 0), 0);

  // Out-of-range indices answer zero / NULL.
  EXPECT_EQ(skity_text_blob_get_run_glyph_count(blob.b, 1), 0);
  EXPECT_EQ(skity_text_blob_get_run_glyphs(blob.b, -1, out_glyphs, 3), 0);
  EXPECT_EQ(skity_text_blob_get_run_pos_x(blob.b, 1, out_x, 3), 0);
  EXPECT_EQ(skity_text_blob_get_run_font(blob.b, 1), nullptr);
  EXPECT_EQ(skity_text_blob_get_run_font(nullptr, 0), nullptr);
}

TEST(CapiSerialization, DelegateCanvasReplaysThroughCallbacks) {
  // Record a display list: matrix / clip / save / draw-rect / draw-circle.
  skity_picture_recorder recorder = skity_picture_recorder_create();
  skity_rect bounds = {0.f, 0.f, 200.f, 200.f};
  skity_picture_recorder_begin(recorder, &bounds);
  CanvasGuard record_canvas{skity_picture_recorder_get_canvas(recorder)};
  PaintGuard paint;

  skity_canvas_translate(record_canvas.c, 2.f, 3.f);
  skity_canvas_save(record_canvas.c);
  skity_rect clip = {0.f, 0.f, 100.f, 100.f};
  skity_canvas_clip_rect(record_canvas.c, &clip, SKITY_CLIP_OP_INTERSECT);
  skity_rect rect = {10.f, 10.f, 60.f, 60.f};
  skity_canvas_draw_rect(record_canvas.c, &rect, paint.p);
  skity_canvas_draw_circle(record_canvas.c, 50.f, 50.f, 20.f, paint.p);
  skity_canvas_restore(record_canvas.c);

  DisplayListGuard list;
  ASSERT_EQ(skity_picture_recorder_finish(recorder, &list.l), SKITY_SUCCESS);
  skity_picture_recorder_destroy(recorder);
  ASSERT_GT(skity_display_list_get_op_count(list.l), 0u);

  // Replay it into a delegate canvas and observe the op stream.
  DelegateSink sink;
  auto callbacks = SinkCallbacks();
  CanvasGuard delegate{skity_canvas_create_delegate(&callbacks, &sink)};
  ASSERT_NE(delegate.c, nullptr);
  skity_display_list_draw(list.l, delegate.c);

  ASSERT_EQ(sink.events.size(), 6u);
  EXPECT_EQ(sink.events[0], "translate");
  EXPECT_EQ(sink.events[1], "save");
  EXPECT_EQ(sink.events[2], "clip_rect");
  EXPECT_EQ(sink.events[3], "draw_rect");
  EXPECT_EQ(sink.events[4], "draw_path");
  EXPECT_EQ(sink.events[5], "restore");

  EXPECT_FLOAT_EQ(sink.last_translate[0], 2.f);
  EXPECT_FLOAT_EQ(sink.last_translate[1], 3.f);
  EXPECT_EQ(sink.last_clip_op, SKITY_CLIP_OP_INTERSECT);
  // The circle was converted to a path by the core before delivery.
  EXPECT_GT(sink.last_path_verb_count, 0);

  EXPECT_EQ(sink.destroyed, 0);
  skity_canvas_destroy(delegate.c);
  delegate.c = nullptr;
  EXPECT_EQ(sink.destroyed, 1);
}

TEST(CapiSerialization, DelegateCanvasDirectCallsAndLifecycle) {
  DelegateSink sink;
  auto callbacks = SinkCallbacks();

  // A NULL callback table is rejected; an all-NULL table is accepted and
  // simply drops the ops.
  EXPECT_EQ(skity_canvas_create_delegate(nullptr, &sink), nullptr);
  {
    skity_canvas_delegate_callbacks silent{};
    CanvasGuard delegate{skity_canvas_create_delegate(&silent, &sink)};
    ASSERT_NE(delegate.c, nullptr);
    PaintGuard paint;
    skity_canvas_save(delegate.c);
    skity_canvas_draw_paint(delegate.c, paint.p);
  }
  EXPECT_TRUE(sink.events.empty());
  EXPECT_EQ(sink.destroyed, 0);

  CanvasGuard delegate{skity_canvas_create_delegate(&callbacks, &sink)};
  ASSERT_NE(delegate.c, nullptr);

  // Driving the delegate through the plain canvas entries fires the matching
  // callbacks with usable borrowed handles.
  PathGuard path;
  skity_path_move_to(path.p, 1.f, 2.f);
  skity_path_line_to(path.p, 3.f, 4.f);
  PaintGuard paint;
  skity_canvas_draw_path(delegate.c, path.p, paint.p);
  ASSERT_EQ(sink.events.size(), 1u);
  EXPECT_EQ(sink.events[0], "draw_path");
  EXPECT_EQ(sink.last_path_verb_count, 2);

  TypefaceGuard typeface{skity_typeface_get_default()};
  if (typeface.t != nullptr) {
    FontGuard font{skity_font_create_with_typeface(typeface.t, 12.f)};
    const uint16_t glyphs[2] = {1, 2};
    BlobGuard blob{skity_text_blob_create_from_glyphs(font.f, glyphs, nullptr,
                                                      nullptr, 2)};
    ASSERT_NE(blob.b, nullptr);
    skity_canvas_draw_text_blob(delegate.c, blob.b, 5.f, 6.f, paint.p);
    ASSERT_EQ(sink.events.size(), 2u);
    EXPECT_EQ(sink.events[1], "draw_blob");
    EXPECT_EQ(sink.last_path_verb_count, 1);  // run count of the blob
  }

  skity_canvas_destroy(delegate.c);
  delegate.c = nullptr;
  EXPECT_EQ(sink.destroyed, 1);
}
