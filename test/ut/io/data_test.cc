// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

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

class ScopedTempFile {
 public:
  explicit ScopedTempFile(const std::string& filename)
      : path_(std::filesystem::temp_directory_path() / filename) {}

  ~ScopedTempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path& Path() const { return path_; }
  std::string PathString() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

// ---------- WriteToFile ----------
TEST(DataTest, WriteToFileRoundTrip) {
  uint8_t buf[] = {10, 20, 30, 40, 50};
  auto data = Data::MakeWithCopy(buf, sizeof(buf));

  ScopedTempFile temp_file("skity_data_write_to_file.bin");
  ASSERT_TRUE(data->WriteToFile(temp_file.PathString().c_str()));

  std::ifstream in(temp_file.Path(), std::ios::in | std::ios::binary);
  ASSERT_TRUE(in.is_open());
  std::vector<uint8_t> read_back((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());

  ASSERT_EQ(read_back.size(), sizeof(buf));
  EXPECT_EQ(std::memcmp(read_back.data(), buf, sizeof(buf)), 0);
}

TEST(DataTest, WriteToFileFailureUnwritablePath) {
  uint8_t buf[] = {1, 2, 3};
  auto data = Data::MakeWithCopy(buf, sizeof(buf));

  EXPECT_FALSE(data->WriteToFile("/nonexistent_dir_xyz/file.bin"));
}

TEST(DataTest, WriteToFileEmptyDataFails) {
  auto data = Data::MakeEmpty();

  ScopedTempFile temp_file("skity_data_write_empty.bin");
  EXPECT_FALSE(data->WriteToFile(temp_file.PathString().c_str()));
}

// ---------- MakeFromFileName ----------
TEST(DataTest, MakeFromFileNameRoundTrip) {
  uint8_t buf[] = {5, 4, 3, 2, 1, 0, 255};
  ScopedTempFile temp_file("skity_data_make_from_filename.bin");

  {
    std::ofstream out(temp_file.Path(), std::ios::out | std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(buf), sizeof(buf));
  }

  auto data = Data::MakeFromFileName(temp_file.PathString().c_str());

  ASSERT_NE(data, nullptr);
  ASSERT_FALSE(data->IsEmpty());
  ASSERT_EQ(data->Size(), sizeof(buf));
  EXPECT_EQ(std::memcmp(data->RawData(), buf, sizeof(buf)), 0);
}

TEST(DataTest, MakeFromFileNameNotFound) {
  auto data = Data::MakeFromFileName("/nonexistent_dir_xyz/does_not_exist.bin");

  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(data->IsEmpty());
  EXPECT_EQ(data->Size(), 0u);
  EXPECT_EQ(data->RawData(), nullptr);
  EXPECT_EQ(data.get(), Data::MakeEmpty().get());
}

// ---------- MakeFromMalloc ----------
TEST(DataTest, MakeFromMallocBasic) {
  const size_t length = 4;
  void* raw = std::malloc(length);
  ASSERT_NE(raw, nullptr);

  uint8_t* bytes = reinterpret_cast<uint8_t*>(raw);
  bytes[0] = 9;
  bytes[1] = 8;
  bytes[2] = 7;
  bytes[3] = 6;

  // Data takes ownership of `raw` and will free it on destruction, so it
  // must not be freed here.
  auto data = Data::MakeFromMalloc(raw, length);

  ASSERT_FALSE(data->IsEmpty());
  EXPECT_EQ(data->Size(), length);
  EXPECT_EQ(data->RawData(), raw);
  EXPECT_EQ(std::memcmp(data->RawData(), bytes, length), 0);
}

// ---------- MakeFromFileMapping ----------
TEST(DataTest, MakeFromFileMappingRoundTrip) {
  uint8_t buf[] = {42, 43, 44, 45, 46};
  ScopedTempFile temp_file("skity_data_make_from_file_mapping.bin");

  {
    std::ofstream out(temp_file.Path(), std::ios::out | std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(buf), sizeof(buf));
  }

  auto data = Data::MakeFromFileMapping(temp_file.PathString().c_str());

  ASSERT_NE(data, nullptr);
  ASSERT_FALSE(data->IsEmpty());
  ASSERT_EQ(data->Size(), sizeof(buf));
  EXPECT_EQ(std::memcmp(data->RawData(), buf, sizeof(buf)), 0);
}

TEST(DataTest, MakeFromFileMappingNotFound) {
  auto data =
      Data::MakeFromFileMapping("/nonexistent_dir_xyz/does_not_exist.bin");

  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(data->IsEmpty());
  EXPECT_EQ(data->Size(), 0u);
  EXPECT_EQ(data->RawData(), nullptr);
  EXPECT_EQ(data.get(), Data::MakeEmpty().get());
}
