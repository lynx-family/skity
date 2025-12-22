// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/io/data.hpp>

using namespace skity;

class AllocatorAutoReset {
 public:
  explicit AllocatorAutoReset(std::unique_ptr<DataAllocator> allocator)
      : allocator_(std::move(allocator)) {
    Data::SetAllocatorForTest(allocator_.get());
  }

  ~AllocatorAutoReset() { Data::SetAllocatorForTest(nullptr); }

 private:
  std::unique_ptr<DataAllocator> allocator_;
};

// ---------- MakeEmpty ----------
TEST(DataTest, MakeEmptyBasic) {
  auto d1 = Data::MakeEmpty();

  EXPECT_EQ(d1->Size(), 0u);
  EXPECT_EQ(d1->RawData(), nullptr);
}

TEST(DataTest, MakeEmptySingleton) {
  auto d1 = Data::MakeEmpty();
  auto d2 = Data::MakeEmpty();

  EXPECT_EQ(d1.get(), d2.get());
}

// ---------- MakeWithCopy ----------
TEST(DataTest, MakeWithCopyValid) {
  uint8_t buf[] = {1, 2, 3, 4};
  auto data = Data::MakeWithCopy(buf, sizeof(buf));

  ASSERT_FALSE(data->IsEmpty());
  EXPECT_EQ(data->Size(), sizeof(buf));
  EXPECT_NE(data->RawData(), buf);
}

TEST(DataTest, MakeWithCopyInvalidArgs) {
  auto d1 = Data::MakeWithCopy(nullptr, 4);
  auto d2 = Data::MakeWithCopy("abc", 0);

  EXPECT_TRUE(d1->IsEmpty());
  EXPECT_TRUE(d2->IsEmpty());
}

TEST(DataTest, MallocFailed) {
  class FailingAllocator : public DataAllocator {
   public:
    void* Malloc(size_t) override { return nullptr; }
    void Free(void*) override {}
  };

  AllocatorAutoReset alloc(std::make_unique<FailingAllocator>());

  uint8_t buf[] = {1, 2, 3, 4};
  auto data = Data::MakeWithCopy(buf, sizeof(buf));
  ASSERT_TRUE(data->IsEmpty());
}

// ---------- MakeWithCString ----------
TEST(DataTest, MakeWithCStringNormal) {
  auto data = Data::MakeWithCString("hello");
  ASSERT_FALSE(data->IsEmpty());
  EXPECT_EQ(data->Size(), 6u);
  EXPECT_STREQ(reinterpret_cast<const char*>(data->RawData()), "hello");
}

TEST(DataTest, MakeWithCStringNull) {
  auto data = Data::MakeWithCString(nullptr);
  ASSERT_FALSE(data->IsEmpty());
  EXPECT_EQ(data->Size(), 1u);
  EXPECT_EQ(*reinterpret_cast<const char*>(data->RawData()), '\0');
}

TEST(DataTest, MakeWithCStringEmpty) {
  auto data = Data::MakeWithCString("");
  ASSERT_FALSE(data->IsEmpty());
  EXPECT_EQ(data->Size(), 1u);
  EXPECT_EQ(*reinterpret_cast<const char*>(data->RawData()), '\0');
}

// ---------- MakeWithProc ----------
TEST(DataTest, MakeWithProcReleaseCalled) {
  bool released = false;
  auto releaseProc = [](const void* ptr, void* ctx) {
    bool* flag = reinterpret_cast<bool*>(ctx);
    *flag = true;
  };

  uint8_t buf[] = {1, 2, 3};
  {
    auto data = Data::MakeWithProc(buf, sizeof(buf), releaseProc, &released);
    EXPECT_FALSE(released);
  }
  EXPECT_TRUE(released);
}
