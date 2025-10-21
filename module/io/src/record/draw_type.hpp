/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_RECORD_DRAW_TYPE_HPP
#define MODULE_IO_SRC_RECORD_DRAW_TYPE_HPP

namespace skity {

/*
 * Note: While adding new DrawTypes, it is necessary to add to the end of this
 * list and update LAST_DRAWTYPE_ENUM to avoid having the code read older skps
 * wrong. (which can cause segfaults)
 *
 *       Reordering can be done during version updates.
 */
enum DrawType {
  UNUSED,
  CLIP_PATH,
  CLIP_REGION,
  CLIP_RECT,
  CLIP_RRECT,
  CONCAT,
  DRAW_BITMAP_RETIRED_2016_REMOVED_2018,
  DRAW_BITMAP_MATRIX_RETIRED_2016_REMOVED_2018,
  DRAW_BITMAP_NINE_RETIRED_2016_REMOVED_2018,
  DRAW_BITMAP_RECT_RETIRED_2016_REMOVED_2018,
  DRAW_CLEAR,
  DRAW_DATA,
  DRAW_OVAL,
  DRAW_PAINT,
  DRAW_PATH,
  DRAW_PICTURE,
  DRAW_POINTS,
  DRAW_POS_TEXT_REMOVED_1_2019,
  DRAW_POS_TEXT_TOP_BOTTOM_REMOVED_1_2019,
  DRAW_POS_TEXT_H_REMOVED_1_2019,
  DRAW_POS_TEXT_H_TOP_BOTTOM_REMOVED_1_2019,
  DRAW_RECT,
  DRAW_RRECT,
  DRAW_SPRITE_RETIRED_2015_REMOVED_2018,
  DRAW_TEXT_REMOVED_1_2019,
  DRAW_TEXT_ON_PATH_RETIRED_08_2018_REMOVED_10_2018,
  DRAW_TEXT_TOP_BOTTOM_REMOVED_1_2019,
  DRAW_VERTICES_RETIRED_03_2017_REMOVED_01_2018,
  RESTORE,
  ROTATE,
  SAVE,
  SAVE_LAYER_SAVEFLAGS_DEPRECATED_2015_REMOVED_12_2020,
  SCALE,
  SET_MATRIX,
  SKEW,
  TRANSLATE,
  NOOP,
  BEGIN_COMMENT_GROUP_obsolete,
  COMMENT_obsolete,
  END_COMMENT_GROUP_obsolete,

  // new ops -- feel free to re-alphabetize on next version bump
  DRAW_DRRECT,
  PUSH_CULL,  // deprecated, M41 was last Chromium version to write this to an
              // .skp
  POP_CULL,   // deprecated, M41 was last Chromium version to write this to an
              // .skp

  DRAW_PATCH,  // could not add in aphabetical order
  DRAW_PICTURE_MATRIX_PAINT,
  DRAW_TEXT_BLOB,
  DRAW_IMAGE,
  DRAW_IMAGE_RECT_STRICT_obsolete,
  DRAW_ATLAS,
  DRAW_IMAGE_NINE,
  DRAW_IMAGE_RECT,

  SAVE_LAYER_SAVELAYERFLAGS_DEPRECATED_JAN_2016_REMOVED_01_2018,
  SAVE_LAYER_SAVELAYERREC,

  DRAW_ANNOTATION,
  DRAW_DRAWABLE,
  DRAW_DRAWABLE_MATRIX,
  DRAW_TEXT_RSXFORM_DEPRECATED_DEC_2018,

  TRANSLATE_Z,  // deprecated (M60)

  DRAW_SHADOW_REC,
  DRAW_IMAGE_LATTICE,
  DRAW_ARC,
  DRAW_REGION,
  DRAW_VERTICES_OBJECT,

  FLUSH,  // no-op

  DRAW_EDGEAA_IMAGE_SET,

  SAVE_BEHIND,

  DRAW_EDGEAA_QUAD,

  DRAW_BEHIND_PAINT,
  CONCAT44,
  CLIP_SHADER_IN_PAINT,
  MARK_CTM,  // deprecated
  SET_M44,

  DRAW_IMAGE2,
  DRAW_IMAGE_RECT2,
  DRAW_IMAGE_LATTICE2,
  DRAW_EDGEAA_IMAGE_SET2,

  RESET_CLIP,

  DRAW_SLUG,

  LAST_DRAWTYPE_ENUM = DRAW_SLUG,
};

}  // namespace skity

#endif  // MODULE_IO_SRC_RECORD_DRAW_TYPE_HPP
