// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_LIST_HPP
#define SRC_UTILS_LIST_HPP

#include <type_traits>

namespace skity {

template <typename T>
struct LinkedList {
  T* head = nullptr;
  T* tail = nullptr;

  LinkedList() = default;
  LinkedList(T* h, T* t) : head(h), tail(t) {}
};

template <class T, T* T::*Prev, T* T::*Next>
void ListInsert(T* t, T* prev, T* next, T** head, T** tail) {
  t->*Prev = prev;
  t->*Next = next;

  if (prev) {
    prev->*Next = t;
  } else if (head) {
    *head = t;
  }

  if (next) {
    next->*Prev = t;
  } else if (tail) {
    *tail = t;
  }
}

template <class T, T* T::*Prev, T* T::*Next>
void ListRemove(T* t, T** head, T** tail) {
  if (t->*Prev) {
    t->*Prev->*Next = t->*Next;
  } else if (head) {
    *head = t->*Next;
  }

  if (t->*Next) {
    t->*Next->*Prev = t->*Prev;
  } else if (tail) {
    *tail = t->*Prev;
  }

  t->*Prev = t->*Next = nullptr;
}

}  // namespace skity

#endif  // SRC_UTILS_LIST_HPP
