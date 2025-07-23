// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_DARWIN_TYPES_DARWIN_HPP
#define SRC_TEXT_PORTS_DARWIN_TYPES_DARWIN_HPP

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include <functional>
#include <memory>
#include <skity/text/font_style.hpp>
#include <type_traits>

namespace skity {

template <typename T, T* P>
struct CFReleaseWrapper {
  template <typename... Args>
  auto operator()(Args&&... args) const
      -> decltype(P(std::forward<Args>(args)...)) {
    return P(std::forward<Args>(args)...);
  }
};

template <typename CFREF>
using UniqueCFRef =
    std::unique_ptr<std::remove_pointer_t<CFREF>,
                    CFReleaseWrapper<decltype(CFRelease), CFRelease>>;

using UniqueCTFontRef = UniqueCFRef<CTFontRef>;
using UniqueCTArrayRef = UniqueCFRef<CFArrayRef>;

void ct_desc_to_font_style(CTFontDescriptorRef desc, FontStyle* style);

}  // namespace skity

#endif  // SRC_TEXT_PORTS_DARWIN_TYPES_DARWIN_HPP
