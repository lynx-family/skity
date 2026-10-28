// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_BASE_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_BASE_HPP

// Header-only RAII layer internals. Everything in the wrapper headers
// defines plain skity:: classes, which is only sound against the unified
// library (SKITY_UNIFIED_LIB): a libskity that still exports the legacy
// C++ API would bind the wrapper's uninlined calls to the legacy
// implementations — see skity.hpp for the full story. Guard that here, at
// the one header every wrapper TU transitively includes.
#if !defined(SKITY_UNIFIED_LIB)
#error \
    "The skity/ wrapper headers define plain skity:: classes over the C ABI " \
    "and can only " \
    "be compiled against a unified libskity (-DSKITY_UNIFIED_LIB=ON, or " \
    "define SKITY_UNIFIED_LIB yourself when consuming an installed " \
    "unified build). Against a normal build the wrapper's inline calls " \
    "would link to the exported legacy C++ implementations."
#endif

namespace skity {
namespace detail {

/**
 * Move-only owner of a C handle. Calls destroy_fn on destruction; copying
 * is disabled so ownership stays unique (matches the legacy API where
 * consumers held unique/shared_ptr to C++ objects). Wrappers of value-type
 * C++ objects (Paint / Path / Font) opt into copy semantics by declaring
 * copy members that clone the underlying object.
 */
template <typename Handle, void (*destroy_fn)(Handle)>
class OwnHandle {
 public:
  OwnHandle() = default;

  explicit OwnHandle(Handle h) : handle_(h) {}

  OwnHandle(OwnHandle&& other) noexcept : handle_(other.release()) {}

  OwnHandle& operator=(OwnHandle&& other) noexcept {
    if (this != &other) {
      destroy();
      handle_ = other.release();
    }
    return *this;
  }

  OwnHandle(const OwnHandle&) = delete;
  OwnHandle& operator=(const OwnHandle&) = delete;

  ~OwnHandle() { destroy(); }

  /** Raw C handle; NULL when empty (creation failed or was released). */
  Handle get() const { return handle_; }

  explicit operator bool() const { return handle_ != nullptr; }

  /** Give up ownership without destroying the handle. */
  Handle release() {
    Handle h = handle_;
    handle_ = nullptr;
    return h;
  }

  /** Destroy the current handle (if any) and take h instead. */
  void reset(Handle h = nullptr) {
    destroy();
    handle_ = h;
  }

 protected:
  void destroy() {
    if (handle_ != nullptr) {
      destroy_fn(handle_);
      handle_ = nullptr;
    }
  }

 private:
  Handle handle_ = nullptr;
};

}  // namespace detail
}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_BASE_HPP
