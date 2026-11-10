# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# Habitat pins Fontconfig 2.17.1 with a generated public header.
skity_add_fontconfig(
  SOURCE_DIR "${SKITY_ROOT}/third_party/fontconfig"
  LIBRARY_TARGET libfontconfig.a
  GENERATED_HEADER
  MESON_OPTIONS -Dxml-backend=expat -Dfontations=disabled)
