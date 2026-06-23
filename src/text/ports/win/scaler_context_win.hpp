// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_WIN_SCALER_CONTEXT_WIN_HPP
#define SRC_TEXT_PORTS_WIN_SCALER_CONTEXT_WIN_HPP

// clang-format off
#include "src/text/ports/win/dwrite_version.hpp"
#include <dwrite.h>
// clang-format on

#include <memory>

#include "src/text/ports/win/scoped_com_ptr.hpp"
#include "src/text/scaler_context.hpp"

namespace skity {

std::unique_ptr<ScalerContext> MakeScalerContextDWrite(
    std::shared_ptr<Typeface> typeface, IDWriteFactory* factory,
    IDWriteFontFace* font_face, const ScalerContextDesc* desc);

}  // namespace skity

#endif  // SRC_TEXT_PORTS_WIN_SCALER_CONTEXT_WIN_HPP
