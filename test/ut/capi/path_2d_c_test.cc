// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity/c/skity_path.h>
#include <skity/c/skity_path_measure.h>

extern "C" int skity_test_path_2d_c_smoke(void);

namespace {
struct PathGuard {
  skity_path path = skity_path_create();
  ~PathGuard() { skity_path_destroy(path); }
};
void ExpectPoint(const skity_vec2& point, float x, float y) {
  EXPECT_FLOAT_EQ(point.x, x);
  EXPECT_FLOAT_EQ(point.y, y);
}
}  // namespace

TEST(CapiPath2D, PureCConsumer) { EXPECT_EQ(skity_test_path_2d_c_smoke(), 0); }

TEST(CapiPath2D, QueriesAndBoundedCopies) {
  PathGuard p;
  skity_vec2 point = {9, 9};
  EXPECT_EQ(skity_path_get_last_pt_2d(p.path, &point), 0u);
  ExpectPoint(point, 0, 0);
  skity_path_get_last_move_pt_2d(p.path, &point);
  ExpectPoint(point, 0, 0);
  skity_path_move_to(p.path, 1, 2);
  skity_path_line_to(p.path, 3, 4);
  EXPECT_EQ(skity_path_get_point_2d(p.path, 1, &point), 1u);
  ExpectPoint(point, 3, 4);
  EXPECT_EQ(skity_path_get_point_2d(p.path, -1, &point), 0u);
  ExpectPoint(point, 0, 0);
  EXPECT_EQ(skity_path_get_point_2d(p.path, 2, &point), 0u);
  ExpectPoint(point, 0, 0);
  EXPECT_EQ(skity_path_get_last_pt_2d(p.path, nullptr), 1u);
  EXPECT_EQ(skity_path_get_last_pt_2d(p.path, &point), 1u);
  ExpectPoint(point, 3, 4);
  skity_path_get_last_move_pt_2d(p.path, &point);
  ExpectPoint(point, 1, 2);
  skity_vec2 points[3] = {{9, 9}, {9, 9}, {9, 9}};
  EXPECT_EQ(skity_path_copy_points_2d(p.path, nullptr, 0), 2u);
  EXPECT_EQ(skity_path_copy_points_2d(p.path, points, 0), 0u);
  ExpectPoint(points[0], 9, 9);
  EXPECT_EQ(skity_path_copy_points_2d(p.path, points, 1), 1u);
  ExpectPoint(points[0], 1, 2);
  ExpectPoint(points[1], 9, 9);
  EXPECT_EQ(skity_path_copy_points_2d(p.path, points, 3), 2u);
  ExpectPoint(points[1], 3, 4);
  ExpectPoint(points[2], 9, 9);
  EXPECT_EQ(skity_path_is_line_2d(p.path, nullptr), 1u);
  EXPECT_EQ(skity_path_is_line_2d(p.path, points), 1u);
  ExpectPoint(points[0], 1, 2);
  ExpectPoint(points[1], 3, 4);
  skity_path_close(p.path);
  EXPECT_EQ(skity_path_is_line_2d(p.path, points), 0u);
  ExpectPoint(points[1], 3, 4);
}

TEST(CapiPath2D, CopiesXYWithoutPerspectiveDivision) {
  PathGuard p;
  skity_path_move_to(p.path, 2, 4);
  skity_path_line_to(p.path, 6, 8);
  skity_matrix matrix = {};
  matrix.m[0] = matrix.m[5] = matrix.m[10] = 1;
  matrix.m[15] = 2;
  skity_path_transform(p.path, &matrix);
  skity_vec2 point;
  EXPECT_EQ(skity_path_get_point_2d(p.path, 0, &point), 1u);
  ExpectPoint(point, 2, 4);
  skity_vec2 points[4] = {};
  EXPECT_EQ(skity_path_copy_points_2d(p.path, points, 4), 2u);
  ExpectPoint(points[1], 6, 8);
  auto iter = skity_path_iter_create(p.path, 0);
  ASSERT_NE(iter, nullptr);
  EXPECT_EQ(skity_path_iter_next_2d(iter, points), SKITY_PATH_VERB_MOVE);
  ExpectPoint(points[0], 2, 4);
  skity_path_iter_destroy(iter);
}

TEST(CapiPath2D, IteratorsPreserveSegmentsWeightsAndClosing) {
  PathGuard p;
  skity_path_move_to(p.path, 1, 2);
  skity_path_line_to(p.path, 3, 4);
  skity_path_quad_to(p.path, 5, 6, 7, 8);
  skity_path_conic_to(p.path, 9, 10, 11, 12, 0.5f);
  skity_path_cubic_to(p.path, 13, 14, 15, 16, 17, 18);
  auto raw = skity_path_raw_iter_create(p.path);
  auto iter = skity_path_iter_create(p.path, 1);
  ASSERT_NE(raw, nullptr);
  ASSERT_NE(iter, nullptr);
  EXPECT_EQ(skity_path_iter_is_closed_contour(iter), 1u);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, nullptr), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_iter_next_2d(iter, nullptr), SKITY_PATH_VERB_DONE);
  const int counts[] = {1, 2, 3, 3, 4};
  const float starts[] = {1, 1, 3, 7, 11};
  for (int v = 0; v < 5; ++v) {
    skity_vec2 a[4] = {{99, 99}, {99, 99}, {99, 99}, {99, 99}};
    skity_vec2 b[4] = {{99, 99}, {99, 99}, {99, 99}, {99, 99}};
    EXPECT_EQ(skity_path_raw_iter_peek(raw), v);
    EXPECT_EQ(skity_path_raw_iter_next_2d(raw, a), v);
    EXPECT_EQ(skity_path_iter_next_2d(iter, b), v);
    for (int i = 0; i < 4; ++i) {
      float x = i < counts[v] ? starts[v] + 2 * i : 99;
      float y = i < counts[v] ? x + 1 : 99;
      ExpectPoint(a[i], x, y);
      ExpectPoint(b[i], x, y);
    }
    if (v == SKITY_PATH_VERB_CONIC) {
      EXPECT_FLOAT_EQ(skity_path_raw_iter_conic_weight(raw), 0.5f);
      EXPECT_FLOAT_EQ(skity_path_iter_conic_weight(iter), 0.5f);
    }
    EXPECT_EQ(skity_path_iter_is_close_line(iter), 0u);
  }
  skity_vec2 points[4] = {{99, 99}, {99, 99}, {99, 99}, {99, 99}};
  EXPECT_EQ(skity_path_raw_iter_peek(raw), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, points), SKITY_PATH_VERB_DONE);
  ExpectPoint(points[0], 99, 99);
  EXPECT_EQ(skity_path_iter_next_2d(iter, points), SKITY_PATH_VERB_LINE);
  EXPECT_EQ(skity_path_iter_is_close_line(iter), 1u);
  ExpectPoint(points[0], 17, 18);
  ExpectPoint(points[1], 1, 2);
  EXPECT_EQ(skity_path_iter_next_2d(iter, points), SKITY_PATH_VERB_CLOSE);
  ExpectPoint(points[0], 17, 18);
  EXPECT_EQ(skity_path_iter_next_2d(iter, points), SKITY_PATH_VERB_DONE);
  skity_path_iter_destroy(iter);
  skity_path_raw_iter_destroy(raw);

  skity_path_close(p.path);
  skity_path_move_to(p.path, 30, 40);
  raw = skity_path_raw_iter_create(p.path);
  for (int i = 0; i < 5; ++i) skity_path_raw_iter_next_2d(raw, points);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, points), SKITY_PATH_VERB_CLOSE);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, points), SKITY_PATH_VERB_MOVE);
  ExpectPoint(points[0], 30, 40);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, points), SKITY_PATH_VERB_DONE);
  skity_path_raw_iter_destroy(raw);
}

TEST(CapiPath2D, MeasureOptionalOutputsAndFailure) {
  PathGuard p;
  auto measure = skity_path_measure_create(p.path, 0, 1);
  ASSERT_NE(measure, nullptr);
  skity_vec2 pos = {9, 9}, tan = {9, 9};
  EXPECT_EQ(skity_path_measure_get_pos_tan_2d(measure, 0, &pos, &tan), 0u);
  ExpectPoint(pos, 9, 9);
  ExpectPoint(tan, 9, 9);
  skity_path_move_to(p.path, 0, 0);
  skity_path_line_to(p.path, 3, 4);
  ASSERT_EQ(skity_path_measure_set_path(measure, p.path, 0), 1u);
  EXPECT_EQ(skity_path_measure_get_pos_tan_2d(measure, 2.5f, &pos, &tan), 1u);
  ExpectPoint(pos, 1.5f, 2);
  ExpectPoint(tan, 0.6f, 0.8f);
  EXPECT_EQ(skity_path_measure_get_pos_tan_2d(measure, -1, &pos, nullptr), 1u);
  ExpectPoint(pos, 0, 0);
  EXPECT_EQ(skity_path_measure_get_pos_tan_2d(measure, 10, &pos, nullptr), 1u);
  ExpectPoint(pos, 3, 4);
  EXPECT_EQ(skity_path_measure_get_pos_tan_2d(measure, 1, nullptr, &tan), 1u);
  EXPECT_EQ(skity_path_measure_get_pos_tan_2d(measure, 1, nullptr, nullptr),
            1u);
  skity_path_measure_destroy(measure);
}

TEST(CapiPath2D, InvalidHandlesAndEmptyIterators) {
  PathGuard p;
  skity_vec2 points[4] = {{9, 9}};
  EXPECT_EQ(skity_path_get_point_2d(nullptr, 0, points), 0u);
  EXPECT_EQ(skity_path_get_point_2d(p.path, 0, nullptr), 0u);
  EXPECT_EQ(skity_path_get_last_pt_2d(nullptr, points), 0u);
  skity_path_get_last_move_pt_2d(nullptr, points);
  EXPECT_EQ(skity_path_copy_points_2d(nullptr, points, 4), 0u);
  EXPECT_EQ(skity_path_is_line_2d(nullptr, points), 0u);
  EXPECT_EQ(skity_path_iter_create(nullptr, 0), nullptr);
  EXPECT_EQ(skity_path_raw_iter_create(nullptr), nullptr);
  EXPECT_EQ(skity_path_iter_next_2d(nullptr, points), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_raw_iter_next_2d(nullptr, points), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_raw_iter_peek(nullptr), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_iter_is_close_line(nullptr), 0u);
  EXPECT_EQ(skity_path_iter_is_closed_contour(nullptr), 0u);
  EXPECT_EQ(skity_path_iter_conic_weight(nullptr), 0.f);
  EXPECT_EQ(skity_path_raw_iter_conic_weight(nullptr), 0.f);
  EXPECT_EQ(skity_path_measure_get_pos_tan_2d(nullptr, 0, points, nullptr), 0u);
  auto iter = skity_path_iter_create(p.path, 0);
  auto raw = skity_path_raw_iter_create(p.path);
  ASSERT_NE(iter, nullptr);
  ASSERT_NE(raw, nullptr);
  EXPECT_EQ(skity_path_iter_next_2d(iter, points), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, points), SKITY_PATH_VERB_DONE);
  EXPECT_EQ(
      skity_path_get_point_2d(reinterpret_cast<skity_path>(iter), 0, points),
      0u);
  EXPECT_EQ(
      skity_path_iter_next_2d(reinterpret_cast<skity_path_iter>(raw), points),
      SKITY_PATH_VERB_DONE);
  EXPECT_EQ(skity_path_raw_iter_next_2d(
                reinterpret_cast<skity_path_raw_iter>(p.path), points),
            SKITY_PATH_VERB_DONE);
  ExpectPoint(points[0], 9, 9);
  skity_path_iter_destroy(iter);
  skity_path_raw_iter_destroy(raw);
  skity_path_iter_destroy(nullptr);
  skity_path_raw_iter_destroy(nullptr);
}

TEST(CapiPath2D, IteratorsCanBeReusedWithoutAllocation) {
  PathGuard first, second;
  skity_path_move_to(first.path, 1, 2);
  skity_path_line_to(first.path, 2, 3);
  skity_path_move_to(second.path, 3, 4);
  skity_path_line_to(second.path, 5, 6);
  auto iter = skity_path_iter_create(first.path, 0);
  auto raw = skity_path_raw_iter_create(first.path);
  ASSERT_NE(iter, nullptr);
  ASSERT_NE(raw, nullptr);
  EXPECT_EQ(skity_path_iter_set_path(iter, nullptr, 1), 0u);
  EXPECT_EQ(skity_path_raw_iter_set_path(raw, nullptr), 0u);
  skity_vec2 points[4] = {};
  EXPECT_EQ(skity_path_iter_next_2d(iter, points), SKITY_PATH_VERB_MOVE);
  ExpectPoint(points[0], 1, 2);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, points), SKITY_PATH_VERB_MOVE);
  ExpectPoint(points[0], 1, 2);
  EXPECT_EQ(skity_path_iter_set_path(iter, second.path, 1), 1u);
  EXPECT_EQ(skity_path_raw_iter_set_path(raw, second.path), 1u);
  EXPECT_EQ(skity_path_iter_is_closed_contour(iter), 1u);
  EXPECT_EQ(skity_path_iter_next_2d(iter, points), SKITY_PATH_VERB_MOVE);
  ExpectPoint(points[0], 3, 4);
  EXPECT_EQ(skity_path_raw_iter_next_2d(raw, points), SKITY_PATH_VERB_MOVE);
  ExpectPoint(points[0], 3, 4);
  EXPECT_EQ(skity_path_iter_set_path(nullptr, second.path, 0), 0u);
  EXPECT_EQ(skity_path_raw_iter_set_path(nullptr, second.path), 0u);
  skity_path_iter_destroy(iter);
  skity_path_raw_iter_destroy(raw);
}
