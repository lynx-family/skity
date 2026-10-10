// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MTL_TYPES_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MTL_TYPES_H

// Objective-C handle types shared by the Metal C API headers
// (skity_context_mtl.h / skity_surface_mtl.h / skity_texture_mtl.h).
//
// The Metal C API is Objective-C-only: the handles are always the real
// Objective-C types (via forward @class / @protocol declarations only,
// so <Metal/Metal.h> is never pulled in) and there is deliberately no
// plain-C / C++ void* fallback. Include the Metal headers only from
// Objective-C(++) translation units; they are not part of the skity.h
// umbrella (the hpp layer guards its Metal wrappers under __OBJC__).
//
// The handles are __unsafe_unretained so plain C structs may embed them
// even when the consumer compiles under ARC.

#if !defined(__OBJC__)
#error "skity's Metal C API is Objective-C-only: include it from a .m / .mm translation unit"
#endif

@class CAMetalLayer;
@protocol MTLDevice;
@protocol MTLCommandQueue;
@protocol MTLTexture;

/** id<MTLDevice> */
typedef __unsafe_unretained id<MTLDevice> skity_mtl_device;
/** id<MTLCommandQueue> */
typedef __unsafe_unretained id<MTLCommandQueue> skity_mtl_command_queue;
/** id<MTLTexture> */
typedef __unsafe_unretained id<MTLTexture> skity_mtl_texture;
/** CAMetalLayer* */
typedef __unsafe_unretained CAMetalLayer* skity_mtl_layer;

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MTL_TYPES_H
