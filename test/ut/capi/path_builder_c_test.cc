// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity/c/skity_path_builder.h>

#include "module/capi/common/handle.hpp"

extern "C" int skity_test_path_builder_c_smoke(void);

namespace {
struct BuilderGuard {
  skity_path_builder b = skity_path_builder_create();
  ~BuilderGuard() { skity_path_builder_destroy(b); }
};
struct PathGuard {
  skity_path p = skity_path_create();
  ~PathGuard() { skity_path_destroy(p); }
};

void ExpectPoint(skity_vec2 point, float x, float y) {
  EXPECT_FLOAT_EQ(point.x, x);
  EXPECT_FLOAT_EQ(point.y, y);
}

TEST(CapiPathBuilder, PureCBuildAndDraw) {
  EXPECT_EQ(skity_test_path_builder_c_smoke(), 0);
}

TEST(CapiPathBuilder, CommandsMatchLegacyPath) {
  BuilderGuard b;
  PathGuard p;
  skity_path_builder_move_to(b.b, 1, 2);
  skity_path_move_to(p.p, 1, 2);
  skity_path_builder_line_to(b.b, 3, 4);
  skity_path_line_to(p.p, 3, 4);
  skity_path_builder_quad_to(b.b, 5, 6, 7, 8);
  skity_path_quad_to(p.p, 5, 6, 7, 8);
  skity_path_builder_conic_to(b.b, 9, 10, 11, 12, 0.5f);
  skity_path_conic_to(p.p, 9, 10, 11, 12, 0.5f);
  skity_path_builder_cubic_to(b.b, 13, 14, 15, 16, 17, 18);
  skity_path_cubic_to(p.p, 13, 14, 15, 16, 17, 18);
  skity_path_builder_set_last_pt(b.b, 20, 21);
  skity_path_set_last_pt(p.p, 20, 21);
  skity_path_builder_close(b.b);
  skity_path_close(p.p);
  skity_path_builder_line_to(b.b, 30, 31);
  skity_path_line_to(p.p, 30, 31);
  skity_path_builder_set_fill_type(b.b, SKITY_PATH_FILL_TYPE_EVEN_ODD);
  skity_path_set_fill_type(p.p, SKITY_PATH_FILL_TYPE_EVEN_ODD);
  EXPECT_EQ(skity_path_builder_count_points(b.b), skity_path_count_points(p.p));
  EXPECT_EQ(skity_path_builder_count_verbs(b.b), skity_path_count_verbs(p.p));
  PathGuard result{skity_path_builder_detach(b.b)};
  EXPECT_EQ(skity_path_is_equal(result.p, p.p), 1u);
  EXPECT_EQ(skity_path_builder_is_empty(b.b), 1u);
  EXPECT_EQ(skity_path_builder_get_fill_type(b.b),
            SKITY_PATH_FILL_TYPE_WINDING);
}

TEST(CapiPathBuilder, ShapesAndArcsMatchLegacyPath) {
  auto check = [](auto command) {
    BuilderGuard b;
    PathGuard p;
    command(b.b, p.p);
    PathGuard out{skity_path_builder_detach(b.b)};
    EXPECT_EQ(skity_path_is_equal(out.p, p.p), 1u);
  };
  skity_rect rect{1, 2, 100, 80};
  float radii[8]{4, 5, 6, 7, 8, 9, 10, 11};
  skity_vec2 radii2[4]{{4, 5}, {6, 7}, {8, 9}, {10, 11}};
  for (auto dir : {SKITY_PATH_DIRECTION_CW, SKITY_PATH_DIRECTION_CCW}) {
    check([&](auto b, auto p) {
      skity_path_builder_add_rect(b, &rect, dir);
      skity_path_add_rect(p, &rect, dir);
    });
    check([&](auto b, auto p) {
      skity_path_builder_add_oval(b, &rect, dir);
      skity_path_add_oval(p, &rect, dir);
    });
    check([&](auto b, auto p) {
      skity_path_builder_add_circle(b, 10, 20, 30, dir);
      skity_path_add_circle(p, 10, 20, 30, dir);
    });
    check([&](auto b, auto p) {
      skity_path_builder_add_round_rect(b, &rect, 10, 12, dir);
      skity_path_add_round_rect(p, &rect, 10, 12, dir);
    });
    check([&](auto b, auto p) {
      skity_path_builder_add_round_rect_radii(b, &rect, radii, dir);
      skity_path_add_round_rect_radii(p, &rect, radii, dir);
    });
    check([&](auto b, auto p) {
      skity_path_builder_add_rrect(b, &rect, radii2, dir);
      skity_path_add_round_rect_radii(p, &rect, radii, dir);
    });
    for (uint32_t start = 0; start < 24; ++start) {
      check([&](auto b, auto p) {
        skity_path_builder_add_rrect_start(b, &rect, radii2, dir, start);
        skity_path_add_round_rect_radii_start(p, &rect, radii, dir, start);
      });
    }
    for (auto size : {SKITY_ARC_SIZE_SMALL, SKITY_ARC_SIZE_LARGE}) {
      check([&](auto b, auto p) {
        skity_path_builder_move_to(b, 1, 2);
        skity_path_move_to(p, 1, 2);
        skity_path_builder_arc_to_svg(b, 20, 30, 45, size, dir, 50, 60);
        skity_path_arc_to_svg(p, 20, 30, 45, size, dir, 50, 60);
      });
    }
  }
  check([&](auto b, auto p) {
    skity_path_builder_move_to(b, 1, 2);
    skity_path_move_to(p, 1, 2);
    skity_path_builder_arc_to(b, 10, 2, 10, 30, 5);
    skity_path_arc_to(p, 10, 2, 10, 30, 5);
  });
  for (float sweep : {-720.f, -90.f, 0.f, 90.f, 360.f, 720.f}) {
    for (uint32_t force : {0u, 1u}) {
      check([&](auto b, auto p) {
        skity_path_builder_arc_to_oval(b, &rect, 15, sweep, force);
        skity_path_arc_to_oval(p, &rect, 15, sweep, force);
      });
    }
  }
}

TEST(CapiPathBuilder, ShapeStartIndices) {
  skity_rect rect{0, 0, 100, 80};
  skity_vec2 corners[4]{{0, 0}, {100, 0}, {100, 80}, {0, 80}};
  skity_vec2 oval_points[4]{{50, 0}, {100, 40}, {50, 80}, {0, 40}};
  for (auto dir : {SKITY_PATH_DIRECTION_CW, SKITY_PATH_DIRECTION_CCW}) {
    for (uint32_t start = 0; start < 12; ++start) {
      BuilderGuard b;
      skity_path_builder_add_rect_start(b.b, &rect, dir, start);
      skity_vec2 out{};
      ASSERT_EQ(skity_path_builder_get_point(b.b, 0, &out), 1u);
      ExpectPoint(out, corners[start % 4].x, corners[start % 4].y);
      skity_path_builder_reset(b.b);
      skity_path_builder_add_oval_start(b.b, &rect, dir, start);
      ASSERT_EQ(skity_path_builder_get_point(b.b, 0, &out), 1u);
      ExpectPoint(out, oval_points[start % 4].x, oval_points[start % 4].y);
    }
  }
}

TEST(CapiPathBuilder, AppendModesAndMatrix) {
  PathGuard source;
  skity_path_move_to(source.p, 1, 2);
  skity_path_conic_to(source.p, 3, 4, 5, 6, 0.5f);
  skity_path_close(source.p);
  skity_matrix matrix{};
  matrix.m[0] = 2;
  matrix.m[5] = 3;
  matrix.m[10] = 1;
  matrix.m[12] = 7;
  matrix.m[13] = 8;
  matrix.m[15] = 2;
  for (auto mode : {SKITY_PATH_ADD_MODE_APPEND, SKITY_PATH_ADD_MODE_EXTEND}) {
    BuilderGuard b;
    PathGuard expected;
    skity_path_builder_line_to(b.b, 9, 10);
    skity_path_line_to(expected.p, 9, 10);
    skity_path_builder_add_path(b.b, source.p, 0, 0, mode);
    skity_path_add_path_with_mode(expected.p, source.p, mode);
    skity_path_builder_add_path_matrix(b.b, source.p, &matrix, mode);
    skity_path_add_path_matrix_with_mode(expected.p, source.p, &matrix, mode);
    skity_path_builder_add_path(b.b, source.p, 10, 20,
                                SKITY_PATH_ADD_MODE_APPEND);
    skity_path_add_path(expected.p, source.p, 10, 20);
    skity_path_builder_reverse_add_path(b.b, source.p);
    skity_path_reverse_add_path(expected.p, source.p);
    PathGuard out{skity_path_builder_detach(b.b)};
    EXPECT_EQ(skity_path_is_equal(out.p, expected.p), 1u);
  }
}

TEST(CapiPathBuilder, ReversePathAndBuilderIncludingSelf) {
  PathGuard source;
  skity_path_move_to(source.p, 1, 2);
  skity_path_line_to(source.p, 3, 4);
  skity_path_conic_to(source.p, 5, 6, 7, 8, 0.5f);
  skity_path_cubic_to(source.p, 9, 10, 11, 12, 13, 14);
  BuilderGuard src{skity_path_builder_create_from_path(source.p)};
  for (int mode = 0; mode < 3; ++mode) {
    BuilderGuard b;
    PathGuard expected;
    if (mode == 2) {
      skity_path_builder_add_path(b.b, source.p, 0, 0,
                                  SKITY_PATH_ADD_MODE_APPEND);
      skity_path_add_path(expected.p, source.p, 0, 0);
    } else {
      skity_path_builder_move_to(b.b, 13, 14);
      skity_path_move_to(expected.p, 13, 14);
    }
    skity_path_cubic_to(expected.p, 11, 12, 9, 10, 7, 8);
    skity_path_conic_to(expected.p, 5, 6, 3, 4, 0.5f);
    skity_path_line_to(expected.p, 1, 2);
    if (mode == 0)
      skity_path_builder_reverse_path_to(b.b, source.p);
    else
      skity_path_builder_reverse_builder_to(b.b, mode == 1 ? src.b : b.b);
    PathGuard out{skity_path_builder_detach(b.b)};
    EXPECT_EQ(skity_path_is_equal(out.p, expected.p), 1u);
  }
}

TEST(CapiPathBuilder, ImportSnapshotAndDetachOwnership) {
  PathGuard source;
  skity_path_add_circle(source.p, 10, 20, 30, SKITY_PATH_DIRECTION_CCW);
  skity_path_set_fill_type(source.p, SKITY_PATH_FILL_TYPE_EVEN_ODD);
  auto* source_object = source.p->impl.get();
  BuilderGuard copied{skity_path_builder_create_from_path(source.p)};
  BuilderGuard moved{skity_path_builder_create_from_path_move(source.p)};
  ASSERT_NE(copied.b, nullptr);
  ASSERT_NE(moved.b, nullptr);
  EXPECT_EQ(source.p->impl.get(), source_object);
  EXPECT_EQ(skity_path_is_empty(source.p), 1u);
  EXPECT_EQ(skity_path_get_fill_type(source.p), SKITY_PATH_FILL_TYPE_WINDING);
  PathGuard snapshot{skity_path_builder_snapshot(copied.b)};
  PathGuard detached{skity_path_builder_detach(moved.b)};
  EXPECT_EQ(skity_path_is_equal(snapshot.p, detached.p), 1u);
  skity_path_builder_line_to(copied.b, 100, 101);
  skity_path_line_to(source.p, 7, 8);
  skity_path_set_last_pt(detached.p, 200, 201);
  skity_path_builder_destroy(copied.b);
  copied.b = nullptr;
  EXPECT_NE(skity_path_count_points(snapshot.p), 0u);
  EXPECT_EQ(skity_path_is_equal(snapshot.p, detached.p), 0u);
  EXPECT_EQ(skity_path_builder_get_fill_type(moved.b),
            SKITY_PATH_FILL_TYPE_WINDING);
  skity_path_builder_line_to(moved.b, 3, 4);
  EXPECT_EQ(skity_path_builder_count_points(moved.b), 2u);
}

TEST(CapiPathBuilder, DetachToPreservesObjectAndCompleteValue) {
  BuilderGuard b;
  PathGuard p;
  auto* object = p.p->impl.get();
  skity_path_builder_reserve(b.b, 64, 64, 16);
  skity_rect rect{0, 0, 100, 80};
  skity_path_builder_add_round_rect(b.b, &rect, 10, 20,
                                    SKITY_PATH_DIRECTION_CCW);
  skity_path_builder_set_fill_type(b.b, SKITY_PATH_FILL_TYPE_EVEN_ODD);
  PathGuard expected{skity_path_builder_snapshot(b.b)};
  EXPECT_EQ(skity_path_builder_detach_to(b.b, p.p), 1u);
  EXPECT_EQ(p.p->impl.get(), object);
  EXPECT_EQ(skity_path_is_equal(p.p, expected.p), 1u);
  EXPECT_EQ(skity_path_builder_is_empty(b.b), 1u);
  for (int i = 0; i < 3; ++i) {
    skity_path_builder_line_to(b.b, i, i + 1);
    ASSERT_EQ(skity_path_builder_detach_to(b.b, p.p), 1u);
    EXPECT_EQ(p.p->impl.get(), object);
    EXPECT_EQ(skity_path_count_points(p.p), 2u);
    EXPECT_EQ(skity_path_get_fill_type(p.p), SKITY_PATH_FILL_TYPE_WINDING);
  }
}

TEST(CapiPathBuilder, QueriesResetAndSwap) {
  BuilderGuard a, b;
  skity_vec2 point{9, 9};
  EXPECT_EQ(skity_path_builder_get_last_pt(a.b, &point), 0u);
  EXPECT_EQ(skity_path_builder_get_last_move_pt(a.b, &point), 0u);
  ExpectPoint(point, 9, 9);
  skity_path_builder_move_to(a.b, 1, 2);
  skity_path_builder_line_to(a.b, 3, 4);
  skity_path_builder_close(a.b);
  skity_path_builder_set_fill_type(a.b, SKITY_PATH_FILL_TYPE_EVEN_ODD);
  EXPECT_EQ(skity_path_builder_get_last_pt(a.b, &point), 1u);
  ExpectPoint(point, 3, 4);
  EXPECT_EQ(skity_path_builder_get_last_move_pt(a.b, &point), 1u);
  ExpectPoint(point, 1, 2);
  skity_rect bounds{};
  EXPECT_EQ(skity_path_builder_get_bounds(a.b, &bounds), 1u);
  EXPECT_FLOAT_EQ(bounds.right, 3);
  EXPECT_FLOAT_EQ(bounds.bottom, 4);
  skity_path_builder_swap(a.b, b.b);
  EXPECT_EQ(skity_path_builder_is_empty(a.b), 1u);
  EXPECT_EQ(skity_path_builder_get_fill_type(b.b),
            SKITY_PATH_FILL_TYPE_EVEN_ODD);
  skity_path_builder_reset(b.b);
  EXPECT_EQ(skity_path_builder_get_fill_type(b.b),
            SKITY_PATH_FILL_TYPE_WINDING);
  EXPECT_EQ(skity_path_builder_is_empty(b.b), 1u);
}

TEST(CapiPathBuilder, InvalidAndBorrowedHandlesDoNotConsumeGeometry) {
  BuilderGuard b;
  PathGuard p;
  skity_path_builder_line_to(b.b, 1, 2);
  skity_path_line_to(p.p, 3, 4);
  auto wrong = reinterpret_cast<skity_path_builder>(p.p);
  EXPECT_EQ(skity_path_builder_detach(wrong), nullptr);
  EXPECT_EQ(skity_path_builder_create_from_path_move(nullptr), nullptr);
  EXPECT_EQ(skity_path_builder_create_from_path(nullptr), nullptr);
  EXPECT_EQ(skity_path_builder_snapshot(nullptr), nullptr);
  EXPECT_EQ(skity_path_builder_detach_to(b.b, nullptr), 0u);
  EXPECT_EQ(
      skity_path_builder_detach_to(b.b, reinterpret_cast<skity_path>(b.b)), 0u);
  EXPECT_EQ(skity_path_builder_detach_to(wrong, p.p), 0u);
  skity_path_builder_destroy(wrong);
  skity_path_builder_line_to(wrong, 10, 20);
  PathGuard borrowed{skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, 0u, p.p->impl)};
  EXPECT_EQ(skity_path_builder_create_from_path_move(borrowed.p), nullptr);
  EXPECT_EQ(skity_path_builder_detach_to(b.b, borrowed.p), 0u);
  BuilderGuard copied{skity_path_builder_create_from_path(borrowed.p)};
  ASSERT_NE(copied.b, nullptr);
  skity_vec2 point{9, 9};
  for (int index : {-1, 2}) {
    EXPECT_EQ(skity_path_builder_get_point(b.b, index, &point), 0u);
    ExpectPoint(point, 9, 9);
  }
  EXPECT_EQ(skity_path_builder_get_point(b.b, 0, nullptr), 0u);
  EXPECT_EQ(skity_path_builder_get_last_pt(wrong, &point), 0u);
  EXPECT_EQ(skity_path_builder_get_bounds(b.b, nullptr), 0u);
  skity_path_builder_swap(b.b, wrong);
  skity_path_builder_add_path_matrix(b.b, p.p, nullptr,
                                     SKITY_PATH_ADD_MODE_APPEND);
  skity_path_builder_add_rect(b.b, nullptr, SKITY_PATH_DIRECTION_CW);
  EXPECT_EQ(skity_path_builder_count_points(b.b), 2u);
  EXPECT_EQ(skity_path_count_points(p.p), 2u);
  skity_path_builder_destroy(nullptr);
}
}  // namespace
