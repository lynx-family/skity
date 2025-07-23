// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/utils/array_list.hpp"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

struct TestObject {
  int a;
  double b;

  TestObject(int a, double b) : a(a), b(b) {}

  bool operator==(const TestObject& other) const {
    return a == other.a && b == other.b;
  }
};

TEST(ArrayList, PushBack) {
  skity::ArrayList<TestObject, 4> array_list;
  auto o = TestObject{1, 1.0};
  array_list.push_back(o);
  ASSERT_EQ(array_list.size(), static_cast<size_t>(1));
  auto o2 = TestObject{2, 2.0};
  array_list.push_back(o2);
  ASSERT_EQ(array_list.size(), static_cast<size_t>(2));
  ASSERT_TRUE(array_list[0] == o);
  ASSERT_TRUE(array_list[1] == o2);
  array_list.pop_back();
  ASSERT_EQ(array_list.size(), static_cast<size_t>(1));
  auto o3 = TestObject{3, 3.0};
  array_list.push_back(o3);
  ASSERT_EQ(array_list.size(), static_cast<size_t>(2));
  ASSERT_TRUE(array_list[0] == o);
  ASSERT_TRUE(array_list[1] == o3);
}

TEST(ArrayList, EmplaceBack) {
  skity::ArrayList<TestObject, 4> array_list;
  auto o = TestObject{1, 1.0};
  array_list.emplace_back(1, 1.0);
  ASSERT_EQ(array_list.size(), static_cast<size_t>(1));
  auto o2 = TestObject{2, 2.0};
  array_list.emplace_back(2, 2.0);
  ASSERT_EQ(array_list.size(), static_cast<size_t>(2));
  ASSERT_TRUE(array_list[0] == o);
  ASSERT_TRUE(array_list[1] == o2);
  array_list.pop_back();
  ASSERT_EQ(array_list.size(), static_cast<size_t>(1));
  auto o3 = TestObject{3, 3.0};
  array_list.emplace_back(3, 3.0);
  ASSERT_EQ(array_list.size(), static_cast<size_t>(2));
  ASSERT_TRUE(array_list[0] == o);
  ASSERT_TRUE(array_list[1] == o3);
}

TEST(ArrayList, PushThenPop) {
  skity::ArrayList<TestObject, 4> array_list;
  for (int i = 0; i < 15; i++) {
    auto o = TestObject{i, static_cast<double>(i)};
    array_list.push_back(o);
    ASSERT_EQ(array_list.size(), static_cast<size_t>(i + 1));
    ASSERT_EQ(array_list[i], o);
  }

  for (int i = 14; i >= 0; i--) {
    auto o = TestObject{i, static_cast<double>(i)};
    ASSERT_EQ(array_list[i], o);
    array_list.pop_back();
    ASSERT_EQ(array_list.size(), static_cast<size_t>(i));
  }
}

TEST(ArrayList, Iterator) {
  skity::ArrayList<TestObject, 4> array_list;
  for (int i = 0; i < 15; i++) {
    auto o = TestObject{i, static_cast<double>(i)};
    array_list.push_back(o);
    ASSERT_EQ(array_list.size(), static_cast<size_t>(i + 1));
    ASSERT_EQ(array_list[i], o);
  }
  int i = 0;
  for (auto o : array_list) {
    ASSERT_EQ(o, array_list[i]);
    ASSERT_EQ(o.a, i);
    i++;
  }
}

TEST(ArrayList, IteratorEmpty) {
  skity::ArrayList<TestObject, 4> array_list;
  int i = 0;
  for (auto o : array_list) {
    i++;
    ASSERT_EQ(o.a, 0);
  }
  ASSERT_EQ(i, 0);
}

TEST(ArrayList, Move) {
  skity::ArrayList<TestObject, 4> array_list;
  for (int i = 0; i < 15; i++) {
    auto o = TestObject{i, static_cast<double>(i)};
    array_list.push_back(o);
    ASSERT_EQ(array_list.size(), static_cast<size_t>(i + 1));
    ASSERT_EQ(array_list[i], o);
  }

  skity::ArrayList<TestObject, 4> array_list2 = std::move(array_list);
  for (int i = 0; i < 15; i++) {
    auto o = TestObject{i, static_cast<double>(i)};
    ASSERT_EQ(array_list2[i], o);
  }

  skity::ArrayList<TestObject, 4> array_list3;
  array_list3 = std::move(array_list2);
  for (int i = 0; i < 15; i++) {
    auto o = TestObject{i, static_cast<double>(i)};
    ASSERT_EQ(array_list3[i], o);
  }
}

TEST(ArrayList, FrontAndBack) {
  skity::ArrayList<int32_t, 4> array_list;
  array_list.push_back(1);
  array_list.push_back(2);
  ASSERT_EQ(array_list.front(), 1);
  ASSERT_EQ(array_list.back(), 2);
  array_list.pop_back();
  ASSERT_EQ(array_list.back(), 1);
  array_list.push_back(3);
  ASSERT_EQ(array_list.back(), 3);
  array_list.pop_back();
  array_list.pop_back();
  array_list.push_back(3);
  array_list.push_back(4);
  ASSERT_EQ(array_list.front(), 3);
  ASSERT_EQ(array_list.back(), 4);
}

class DestructibleObj {
 public:
  DestructibleObj(int32_t value, std::function<void(int32_t value)> func)
      : value(value), func(func) {}

  int32_t value;
  ~DestructibleObj() { func(value); }

  std::function<void(int32_t value)> func;
};

TEST(ArrayList, CanCallDestructor) {
  skity::ArrayList<DestructibleObj, 4> array_list;
  std::vector<int32_t> result;
  auto func = [&result](int32_t value) mutable { result.push_back(value); };
  array_list.emplace_back(1, func);
  array_list.emplace_back(2, func);
  array_list.pop_back();
  ASSERT_EQ(result[0], 2);
  result.clear();

  array_list.emplace_back(3, func);
  array_list.emplace_back(4, func);
  array_list.pop_back();
  ASSERT_EQ(result[0], 4);
  result.clear();

  array_list.reset();
  ASSERT_EQ(result[0], 3);
  ASSERT_EQ(result[1], 1);
}

#ifndef SKITY_DEBUG_ARRAY_LIST
TEST(ArrayList, Reset) {
  skity::ArrayList<int32_t, 4> array_list;
  array_list.push_back(1);
  array_list.push_back(2);
  ASSERT_EQ(array_list.size(), static_cast<size_t>(2));
  ASSERT_NE(array_list.GetHeader(), nullptr);
  ASSERT_NE(array_list.GetTail(), nullptr);
  array_list.reset();
  ASSERT_EQ(array_list.size(), static_cast<size_t>(0));
  ASSERT_EQ(array_list.GetHeader(), nullptr);
  ASSERT_EQ(array_list.GetTail(), nullptr);
}

TEST(ArrayList, SetArenaAllocator) {
  skity::ArrayList<int32_t, 4> array_list;
  skity::ArenaAllocator arena_allocator;
  array_list.SetArenaAllocator(&arena_allocator);
  array_list.push_back(1);
  array_list.push_back(2);
  array_list.push_back(3);
  array_list.push_back(4);
  ASSERT_EQ(arena_allocator.GetArena().GetBlocks().size(),
            static_cast<size_t>(1));
  ASSERT_EQ(
      static_cast<size_t>(arena_allocator.GetArena().GetCursor() -
                          arena_allocator.GetArena().GetBlocks().back().head),
      sizeof(skity::ArrayList<int32_t, 4>::Node));
  array_list.push_back(5);
  ASSERT_EQ(
      static_cast<size_t>(arena_allocator.GetArena().GetCursor() -
                          arena_allocator.GetArena().GetBlocks().back().head),
      2 * sizeof(skity::ArrayList<int32_t, 4>::Node));
}
#endif
