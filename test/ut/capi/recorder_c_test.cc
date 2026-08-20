// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Unit tests for the recorder / display-list surface of the C API
// (module/capi). Everything here is CPU-only: display lists are replayed into
// another PictureRecorder's canvas and inspected via op counts / offsets.

#include <skity_c/skity_canvas.h>
#include <skity_c/skity_paint.h>
#include <skity_c/skity_recorder.h>
#include <skity_c/skity_shader.h>

#include <gtest/gtest.h>

namespace {

// RAII guards for the C handles used in these tests.
struct RecorderGuard {
  skity_picture_recorder r = skity_picture_recorder_create();
  ~RecorderGuard() { skity_picture_recorder_destroy(r); }
};

struct PaintGuard {
  skity_paint p = skity_paint_create();
  ~PaintGuard() { skity_paint_destroy(p); }
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

struct ShaderGuard {
  skity_shader s = nullptr;
  ~ShaderGuard() { skity_shader_destroy(s); }
};

constexpr skity_rect kBounds = {0.f, 0.f, 1000.f, 1000.f};

// Records `draws` separated rects (20x20 each, spaced 60 apart) into a
// display list built with the requested rtree option.
skity_display_list RecordSeparatedRects(uint32_t draws, uint32_t build_rtree,
                                        int32_t* last_offset = nullptr) {
  RecorderGuard recorder;
  skity_display_list_build_options options;
  options.build_rtree = build_rtree;
  skity_picture_recorder_begin_with_options(recorder.r, &kBounds, &options);

  skity_canvas canvas = skity_picture_recorder_get_canvas(recorder.r);
  PaintGuard paint;
  for (uint32_t i = 0; i < draws; i++) {
    float x = 20.f + i * 60.f;
    skity_rect rect = {x, 20.f, x + 20.f, 40.f};
    skity_canvas_draw_rect(canvas, &rect, paint.p);
  }
  if (last_offset != nullptr) {
    *last_offset = skity_picture_recorder_get_last_op_offset(recorder.r);
  }

  DisplayListGuard out;
  EXPECT_EQ(skity_picture_recorder_finish(recorder.r, &out.l), SKITY_SUCCESS);
  return out.release();
}

// Replays @p list into a fresh recording canvas and returns the target's
// finished display list, whose op count is then comparable.
skity_display_list ReplayIntoRecordingCanvas(skity_display_list list,
                                             const skity_rect* cull_rect) {
  RecorderGuard target;
  skity_picture_recorder_begin(target.r, &kBounds);
  skity_canvas canvas = skity_picture_recorder_get_canvas(target.r);
  if (cull_rect != nullptr) {
    skity_display_list_draw_with_cull_rect(list, canvas, cull_rect);
  } else {
    skity_display_list_draw(list, canvas);
  }
  DisplayListGuard out;
  EXPECT_EQ(skity_picture_recorder_finish(target.r, &out.l), SKITY_SUCCESS);
  return out.release();
}

}  // namespace

TEST(CapiRecorder, GetLastOpOffsetBeforeBeginIsInvalid) {
  RecorderGuard recorder;
  EXPECT_EQ(skity_picture_recorder_get_last_op_offset(recorder.r), -1);
}

TEST(CapiRecorder, BeginWithOptionsBuildsSearchableList) {
  int32_t last_offset = -1;
  DisplayListGuard list{RecordSeparatedRects(3, 1, &last_offset)};
  ASSERT_NE(list.l, nullptr);
  EXPECT_GE(last_offset, 0);

  // Covering only the last rect matches exactly one op.
  skity_rect last = {140.f, 20.f, 160.f, 40.f};
  int32_t offsets[4] = {-1, -1, -1, -1};
  EXPECT_EQ(skity_display_list_search(list.l, &last, offsets, 4), 1u);
  EXPECT_EQ(offsets[0], last_offset);

  // The whole canvas matches all three, in recording order.
  EXPECT_EQ(skity_display_list_search(list.l, &kBounds, offsets, 4), 3u);
  EXPECT_TRUE(offsets[0] < offsets[1] && offsets[1] < offsets[2]);

  // Capacity truncates the fill but not the reported count.
  offsets[0] = offsets[1] = offsets[2] = offsets[3] = -1;
  EXPECT_EQ(skity_display_list_search(list.l, &kBounds, offsets, 1), 3u);
  EXPECT_EQ(offsets[1], -1);  // untouched beyond capacity

  // A count-only query works without an output buffer.
  EXPECT_EQ(skity_display_list_search(list.l, &kBounds, nullptr, 0), 3u);

  // A region with no draws matches nothing.
  skity_rect empty_region = {0.f, 500.f, 1000.f, 600.f};
  EXPECT_EQ(skity_display_list_search(list.l, &empty_region, offsets, 4), 0u);
}

TEST(CapiRecorder, SearchWithoutRTreeReturnsNothing) {
  DisplayListGuard list{RecordSeparatedRects(2, 0)};
  ASSERT_NE(list.l, nullptr);
  EXPECT_EQ(skity_display_list_search(list.l, &kBounds, nullptr, 0), 0u);
  EXPECT_EQ(skity_display_list_search_non_overlapping_drawn_rects(
                list.l, &kBounds, nullptr, 0),
            0u);
}

TEST(CapiRecorder, SearchNonOverlappingDrawnRectsMergesAdjacent) {
  RecorderGuard recorder;
  skity_display_list_build_options options;
  options.build_rtree = 1;
  skity_picture_recorder_begin_with_options(recorder.r, &kBounds, &options);
  skity_canvas canvas = skity_picture_recorder_get_canvas(recorder.r);
  PaintGuard paint;
  skity_rect left = {20.f, 20.f, 40.f, 40.f};
  skity_rect right = {40.f, 20.f, 60.f, 40.f};  // shares the edge with left
  skity_canvas_draw_rect(canvas, &left, paint.p);
  skity_canvas_draw_rect(canvas, &right, paint.p);
  DisplayListGuard list;
  ASSERT_EQ(skity_picture_recorder_finish(recorder.r, &list.l), SKITY_SUCCESS);

  skity_rect rects[2];
  uint32_t count = skity_display_list_search_non_overlapping_drawn_rects(
      list.l, &kBounds, rects, 2);
  ASSERT_EQ(count, 1u);
  EXPECT_FLOAT_EQ(rects[0].left, 20.f);
  EXPECT_FLOAT_EQ(rects[0].top, 20.f);
  EXPECT_FLOAT_EQ(rects[0].right, 60.f);
  EXPECT_FLOAT_EQ(rects[0].bottom, 40.f);
}

TEST(CapiRecorder, DrawWithCullRectReplaysFewerOps) {
  DisplayListGuard list{RecordSeparatedRects(3, 1)};
  ASSERT_NE(list.l, nullptr);

  skity_rect last_rect = {140.f, 20.f, 160.f, 40.f};
  DisplayListGuard full{ReplayIntoRecordingCanvas(list.l, nullptr)};
  DisplayListGuard partial{ReplayIntoRecordingCanvas(list.l, &last_rect)};
  ASSERT_NE(full.l, nullptr);
  ASSERT_NE(partial.l, nullptr);

  EXPECT_EQ(skity_display_list_get_op_count(full.l), 3u);
  EXPECT_EQ(skity_display_list_get_op_count(partial.l), 1u);

  // A NULL cull rect draws nothing.
  RecorderGuard null_target;
  skity_picture_recorder_begin(null_target.r, &kBounds);
  skity_canvas null_canvas = skity_picture_recorder_get_canvas(null_target.r);
  skity_display_list_draw_with_cull_rect(list.l, null_canvas, nullptr);
  DisplayListGuard null_list;
  ASSERT_EQ(skity_picture_recorder_finish(null_target.r, &null_list.l),
            SKITY_SUCCESS);
  EXPECT_EQ(skity_display_list_get_op_count(null_list.l), 0u);
}

TEST(CapiRecorder, DrawWithCullRectWithoutRTreeReplaysAll) {
  DisplayListGuard list{RecordSeparatedRects(3, 0)};
  ASSERT_NE(list.l, nullptr);

  skity_rect last_rect = {140.f, 20.f, 160.f, 40.f};
  DisplayListGuard culled{ReplayIntoRecordingCanvas(list.l, &last_rect)};
  DisplayListGuard full{ReplayIntoRecordingCanvas(list.l, nullptr)};
  // Without an rtree the cull rect is ignored and everything is replayed.
  EXPECT_EQ(skity_display_list_get_op_count(culled.l),
            skity_display_list_get_op_count(full.l));
}

TEST(CapiRecorder, PropertiesReflectRecordedPaints) {
  RecorderGuard recorder;
  skity_display_list_build_options options;
  options.build_rtree = 1;
  skity_rect bounds = {0.f, 0.f, 100.f, 100.f};
  skity_picture_recorder_begin_with_options(recorder.r, &bounds, &options);
  skity_canvas canvas = skity_picture_recorder_get_canvas(recorder.r);

  PaintGuard paint;
  skity_color4f red = {{1.f, 0.f, 0.f, 1.f}};
  skity_point pts[2] = {{{0.f, 0.f, 0.f, 1.f}}, {{100.f, 0.f, 0.f, 1.f}}};
  ShaderGuard shader{skity_shader_create_linear(pts, &red, nullptr, 2,
                                                SKITY_TILE_MODE_CLAMP, 0)};
  ASSERT_NE(shader.s, nullptr);
  skity_paint_set_shader(paint.p, shader.s);

  skity_rect rect = {10.f, 10.f, 50.f, 50.f};
  skity_canvas_draw_rect(canvas, &rect, paint.p);
  skity_canvas_save_layer(canvas, &bounds, paint.p);

  DisplayListGuard list;
  ASSERT_EQ(skity_picture_recorder_finish(recorder.r, &list.l), SKITY_SUCCESS);

  uint32_t props = skity_display_list_get_properties(list.l);
  EXPECT_TRUE(props & SKITY_DISPLAY_LIST_PROPERTY_SHADER);
  EXPECT_TRUE(props & SKITY_DISPLAY_LIST_PROPERTY_SAVE_LAYER);
  EXPECT_FALSE(props & SKITY_DISPLAY_LIST_PROPERTY_COLOR_FILTER);
  EXPECT_FALSE(props & SKITY_DISPLAY_LIST_PROPERTY_MASK_FILTER);
  EXPECT_FALSE(props & SKITY_DISPLAY_LIST_PROPERTY_IMAGE_FILTER);
}

TEST(CapiRecorder, GetOpPaintByOffset) {
  int32_t offset = -1;
  DisplayListGuard list{RecordSeparatedRects(1, 1, &offset)};
  ASSERT_NE(list.l, nullptr);
  ASSERT_GE(offset, 0);

  // Non-owning paint handle lives inside the display list and is mutable.
  skity_paint paint = skity_display_list_get_op_paint_by_offset(list.l, offset);
  ASSERT_NE(paint, nullptr);
  EXPECT_EQ(skity_paint_get_color(paint), 0xFF000000u);
  skity_paint_set_color(paint, 0xFFFF0000u);

  // A second lookup observes the in-place edit (same underlying Paint).
  skity_paint paint2 =
      skity_display_list_get_op_paint_by_offset(list.l, offset);
  ASSERT_NE(paint2, nullptr);
  EXPECT_EQ(skity_paint_get_color(paint2), 0xFFFF0000u);
  skity_paint_destroy(paint);
  skity_paint_destroy(paint2);

  // Invalid offsets return NULL.
  EXPECT_EQ(skity_display_list_get_op_paint_by_offset(list.l, -1), nullptr);
  EXPECT_EQ(skity_display_list_get_op_paint_by_offset(list.l, 1 << 30),
            nullptr);
}
