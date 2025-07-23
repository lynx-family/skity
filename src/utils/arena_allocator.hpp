// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_ARENA_ALLOCATOR_HPP
#define SRC_UTILS_ARENA_ALLOCATOR_HPP

#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace skity {

static constexpr size_t kDefaultBlockSize = 4 * 1024;

using AllocProc = std::function<void*(size_t)>;
using FreeProc = std::function<void(void*)>;

struct Block {
  uint8_t* head;
  size_t size;
};

class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual Block Alloc(size_t size) = 0;
  virtual void Free(Block& block) = 0;
};

class DefaultAllocator : public Allocator {
 public:
  static std::shared_ptr<Allocator> GetInstance() {
    static std::shared_ptr<DefaultAllocator> instance =
        std::make_shared<DefaultAllocator>();
    return instance;
  }

  Block Alloc(size_t size) override;

  void Free(Block& block) override;
};

class BlockCacheAllocator : public Allocator {
 public:
  BlockCacheAllocator();

  explicit BlockCacheAllocator(std::shared_ptr<Allocator> internal);

  ~BlockCacheAllocator();

  Block Alloc(size_t size) override;

  void Free(Block& block) override;

  const std::vector<Block>& GetBlocks() const { return blocks_; }

 private:
  std::shared_ptr<Allocator> internal_;
  std::vector<Block> blocks_;
};

class Arena {
 public:
  Arena(size_t block_size = kDefaultBlockSize,
        std::shared_ptr<Allocator> allocator = DefaultAllocator::GetInstance());
  ~Arena();

  void Reset();

  uint8_t* Allocate(size_t bytes, size_t align);

  const std::vector<Block>& GetBlocks() const { return blocks_; }
  const uint8_t* GetCursor() const { return cursor_; }
  const uint8_t* GetEnd() const { return end_; }
  size_t GetBlockSize() const { return block_size_; }

 private:
  std::vector<Block> blocks_;
  uint8_t* cursor_;
  uint8_t* end_;
  size_t block_size_;
  std::shared_ptr<Allocator> allocator_;
};

class ArenaAllocator {
 public:
  explicit ArenaAllocator(
      std::shared_ptr<Allocator> allocator = DefaultAllocator::GetInstance())
      : arena_(kDefaultBlockSize, allocator) {}

  template <typename T>
  static void Destruct(void* p) {
    reinterpret_cast<T*>(p)->~T();
  }

  struct Finalizer {
    void (*func)(void* p);
    void* ptr;
    Finalizer* next;
  };

  ~ArenaAllocator();

  template <typename T, typename... Args>
  T* Make(Args&&... args) {
    if constexpr (sizeof...(Args) == 0 && std::is_standard_layout<T>::value &&
                  std::is_trivial<T>::value) {
      uint8_t* p = arena_.Allocate(sizeof(T), alignof(T));
      return reinterpret_cast<T*>(p);
    } else if constexpr (std::is_trivially_destructible<T>::value) {
      uint8_t* p = arena_.Allocate(sizeof(T), alignof(T));
      return new (p) T{std::forward<Args>(args)...};
    } else {
      uint8_t* p = arena_.Allocate(sizeof(T), alignof(T));
      T* o = new (p) T{std::forward<Args>(args)...};
      Finalizer* finalizer = reinterpret_cast<Finalizer*>(
          arena_.Allocate(sizeof(Finalizer), alignof(Finalizer)));
      finalizer->func = Destruct<T>;
      finalizer->ptr = o;
      finalizer->next = finalizer_head_;
      finalizer_head_ = finalizer;
      return o;
    }
  }

  const Arena& GetArena() const { return arena_; }

  const Finalizer* GetFinalizerHead() const { return finalizer_head_; }

  size_t GetFinalizersCount() const {
    size_t count = 0;
    Finalizer* curr = finalizer_head_;
    while (curr != nullptr) {
      curr = curr->next;
      count++;
    }
    return count;
  }

  void Reset();

 private:
  Finalizer* finalizer_head_ = nullptr;
  Arena arena_;
};
}  // namespace skity

#endif  // SRC_UTILS_ARENA_ALLOCATOR_HPP
