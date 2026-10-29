// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity_path.h>
#include <skity/c/skity_path_measure.h>

int skity_test_path_2d_c_smoke(void) {
  skity_path path = skity_path_create();
  if (!path) return 1;
  skity_path_move_to(path, 1, 2);
  skity_path_line_to(path, 3, 4);
  skity_vec2 points[4] = {0};
  skity_path_iter iter = skity_path_iter_create(path, 0);
  int ok = iter && skity_path_copy_points_2d(path, points, 4) == 2 &&
           points[1].x == 3 && points[1].y == 4 &&
           skity_path_iter_next_2d(iter, points) == SKITY_PATH_VERB_MOVE &&
           points[0].x == 1 && points[0].y == 2;
  skity_path_iter_destroy(iter);
  skity_path_destroy(path);
  return ok ? 0 : 2;
}
