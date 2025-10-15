/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_WIN_SCOPED_COM_PTR_HPP
#define SRC_TEXT_PORTS_WIN_SCOPED_COM_PTR_HPP

#include <cassert>

#include "src/base/platform/win/lean_windows.hpp"

// STDMETHOD uses COM_DECLSPEC_NOTHROW, but STDMETHODIMP does not. This leads to
// attribute mismatch between interfaces and implementations which produces
// warnings. In theory a COM component should never throw a c++ exception, but
// COM_DECLSPEC_NOTHROW allows tweaking that (as it may be useful for internal
// only implementations within a single project). The behavior of the attribute
// nothrow and the keyword noexcept are slightly different, so use
// COM_DECLSPEC_NOTHROW instead of noexcept. Older interfaces like IUnknown and
// IStream do not currently specify COM_DECLSPEC_NOTHROW, but it is not harmful
// to mark the implementation more exception strict than the interface.

#define SK_STDMETHODIMP COM_DECLSPEC_NOTHROW STDMETHODIMP
#define SK_STDMETHODIMP_(type) COM_DECLSPEC_NOTHROW STDMETHODIMP_(type)

template <typename T>
T* RefComPtr(T* ptr) {
  ptr->AddRef();
  return ptr;
}

template <typename T>
T* SafeRefComPtr(T* ptr) {
  if (ptr) {
    ptr->AddRef();
  }
  return ptr;
}

template <typename T>
class ScopedComPtr {
 private:
  T* fPtr;

 public:
  constexpr ScopedComPtr() : fPtr(nullptr) {}
  constexpr explicit ScopedComPtr(std::nullptr_t) : fPtr(nullptr) {}
  explicit ScopedComPtr(T* ptr) : fPtr(ptr) {}
  ScopedComPtr(ScopedComPtr&& that) : fPtr(that.release()) {}
  ScopedComPtr(const ScopedComPtr&) = delete;

  ~ScopedComPtr() { this->reset(); }

  ScopedComPtr& operator=(ScopedComPtr&& that) {
    this->reset(that.release());
    return *this;
  }
  ScopedComPtr& operator=(const ScopedComPtr&) = delete;
  ScopedComPtr& operator=(std::nullptr_t) {
    this->reset();
    return *this;
  }

  T& operator*() const {
    SkASSERT(fPtr != nullptr);
    return *fPtr;
  }

  explicit operator bool() const { return fPtr != nullptr; }

  T* operator->() const { return fPtr; }

  /**
   * Returns the address of the underlying pointer.
   * This is dangerous -- it breaks encapsulation and the reference escapes.
   * Must only be used on instances currently pointing to NULL,
   * and only to initialize the instance.
   */
  T** operator&() {  // NOLINT
    assert(fPtr == nullptr);
    return &fPtr;
  }

  T* get() const { return fPtr; }

  void reset(T* ptr = nullptr) {
    if (fPtr) {
      fPtr->Release();
    }
    fPtr = ptr;
  }

  void swap(ScopedComPtr<T>& that) {
    T* temp = this->fPtr;
    this->fPtr = that.fPtr;
    that.fPtr = temp;
  }

  T* release() {
    T* temp = this->fPtr;
    this->fPtr = nullptr;
    return temp;
  }
};

#endif  // SRC_TEXT_PORTS_WIN_SCOPED_COM_PTR_HPP
