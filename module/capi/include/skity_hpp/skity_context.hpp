// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_HPP

// GPU context wrapper of the header-only RAII layer: root of the GPU
// backend, owns surfaces. See skity.hpp for the layer's design.

#include <skity_c/skity_context.h>

#include <skity_hpp/skity_base.hpp>

namespace skity {
namespace raii {

/** GPU context wrapper: root of the GPU backend, owns surfaces. */
class Context : public detail::OwnHandle<skity_context, skity_context_destroy> {
 public:
  /**
   * Create an OpenGL / OpenGL ES context. @p get_proc loads GL symbol
   * addresses (glad GLADloadfunc contract); a live GL context is required.
   * On failure @p out is left empty and the result code is returned.
   */
  static skity_result CreateGL(skity_gl_get_proc get_proc, Context* out) {
    skity_context handle = nullptr;
    skity_result result = skity_context_create_gl(get_proc, &handle);
    if (result == SKITY_SUCCESS && out != nullptr) {
      out->reset(handle);
    }
    return result;
  }
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_HPP
