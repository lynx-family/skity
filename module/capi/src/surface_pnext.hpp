// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_SRC_SURFACE_PNEXT_HPP
#define MODULE_CAPI_SRC_SURFACE_PNEXT_HPP

#include <skity/c/skity_types.h>

namespace skity {
namespace capi {

// Every struct in a surface p_next chain begins with {s_type, p_next}; this
// prefix view walks the chain without knowing each struct's full layout.
struct SurfaceChainNode {
  skity_structure_type s_type;
  const void* p_next;
};

inline skity_structure_type SurfaceChainNodeType(const void* node) {
  return static_cast<const SurfaceChainNode*>(node)->s_type;
}

inline const void* SurfaceChainNodeNext(const void* node) {
  return static_cast<const SurfaceChainNode*>(node)->p_next;
}

// First node of the given type in the chain, or nullptr.
inline const void* FindSurfaceChainNode(const void* p_next,
                                        skity_structure_type type) {
  for (const void* node = p_next; node != nullptr;
       node = SurfaceChainNodeNext(node)) {
    if (SurfaceChainNodeType(node) == type) {
      return node;
    }
  }
  return nullptr;
}

}  // namespace capi
}  // namespace skity

#endif  // MODULE_CAPI_SRC_SURFACE_PNEXT_HPP
