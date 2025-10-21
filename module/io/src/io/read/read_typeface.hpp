// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef MODULE_IO_SRC_IO_READ_READ_TYPEFACE_HPP
#define MODULE_IO_SRC_IO_READ_READ_TYPEFACE_HPP

#include <skity/io/stream.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>

namespace skity {
namespace io {

std::shared_ptr<Typeface> TypefaceMakeFromStream(ReadStream& stream);

}  // namespace io
}  // namespace skity

#endif  // MODULE_IO_SRC_IO_READ_READ_TYPEFACE_HPP
