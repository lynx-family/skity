// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BASE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BASE_HPP

// Header-only RAII layer internals. Everything in the skity_hpp/ headers
// lives in namespace skity::raii (NOT skity::) — see skity.hpp for why this
// is load-bearing while libskity still exports the legacy C++ symbols.

namespace skity {
namespace raii {
namespace detail {

/**
 * Move-only owner of a C handle. Calls destroy_fn on destruction; copying
 * is disabled so ownership stays unique (matches the legacy API where
 * consumers held unique/shared_ptr to C++ objects).
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

  /** Destroy the current handle (if any) and take @p h instead. */
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
}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_BASE_HPP
