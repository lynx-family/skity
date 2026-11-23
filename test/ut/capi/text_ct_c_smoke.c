// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity_text_ct.h>

int skity_test_text_ct_c_smoke(void) {
  /* A NULL font must be rejected by the C entry itself: the C++ bridge it
     forwards to retains the font unconditionally and would crash instead. */
  if (skity_typeface_make_from_ct_font(NULL) != NULL) return 1;
  if (skity_typeface_make_from_ct_font_without_cache(NULL) != NULL) return 2;
  return 0;
}
