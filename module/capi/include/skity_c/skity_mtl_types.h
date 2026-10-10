// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MTL_TYPES_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MTL_TYPES_H

// Objective-C handle types shared by the Metal C API headers
// (skity_context_mtl.h / skity_surface_mtl.h / skity_texture_mtl.h).
//
// When an Objective-C(++) compiler processes this header (__OBJC__), the
// handles use their real Objective-C types, so Apple-side callers can pass
// CAMetalLayer* / id<MTLDevice> / ... without any bridging cast. Only
// forward declarations are used, so the header never pulls in
// <Metal/Metal.h> and stays lightweight. The handles are
// __unsafe_unretained so plain C structs may embed them even when the
// consumer compiles under ARC.
//
// In plain C / C++ mode every handle degrades to void*: the ABI is
// identical (Objective-C object pointers are pointer-sized), so non-Apple
// consumers still compile. Bridge such a value back with
// `(__bridge id<MTLDevice>)ptr` from Objective-C(++) code.

#ifdef __OBJC__
@class CAMetalLayer;
@protocol MTLDevice;
@protocol MTLCommandQueue;
@protocol MTLTexture;

/** id<MTLDevice>; void* when compiled without Objective-C. */
typedef __unsafe_unretained id<MTLDevice> skity_mtl_device;
/** id<MTLCommandQueue>; void* when compiled without Objective-C. */
typedef __unsafe_unretained id<MTLCommandQueue> skity_mtl_command_queue;
/** id<MTLTexture>; void* when compiled without Objective-C. */
typedef __unsafe_unretained id<MTLTexture> skity_mtl_texture;
/** CAMetalLayer*; void* when compiled without Objective-C. */
typedef __unsafe_unretained CAMetalLayer* skity_mtl_layer;
#else
typedef void* skity_mtl_device;        /**< opaque id<MTLDevice> */
typedef void* skity_mtl_command_queue; /**< opaque id<MTLCommandQueue> */
typedef void* skity_mtl_texture;       /**< opaque id<MTLTexture> */
typedef void* skity_mtl_layer;         /**< opaque CAMetalLayer* */
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MTL_TYPES_H
