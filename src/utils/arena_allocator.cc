// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/utils/arena_allocator.hpp"

#include "src/logging.hpp"

namespace skity {

Block DefaultAllocator::Alloc(size_t size) {
  return Block{static_cast<uint8_t*>(std::malloc(size)), size};
}

void DefaultAllocator::Free(Block& block) { return std::free(block.head); }

BlockCacheAllocator::BlockCacheAllocator()
    : internal_(DefaultAllocator::Create()) {}

BlockCacheAllocator::BlockCacheAllocator(std::shared_ptr<Allocator> internal)
    : internal_(std::move(internal)) {}

BlockCacheAllocator::~BlockCacheAllocator() {
  for (Block& block : blocks_) {
    internal_->Free(block);
  }
}

Block BlockCacheAllocator::Alloc(size_t size) {
  if (size == kDefaultBlockSize && !blocks_.empty()) {
    auto result = blocks_.back();
    blocks_.pop_back();
    return result;
  }
  return internal_->Alloc(size);
}

void BlockCacheAllocator::Free(Block& block) {
  if (block.size == kDefaultBlockSize) {
    blocks_.push_back(block);
    return;
  }
  internal_->Free(block);
}

Arena::Arena(size_t block_size, std::shared_ptr<Allocator> allocator)
    : cursor_(nullptr),
      end_(nullptr),
      block_size_(block_size),
      allocator_(allocator != nullptr ? allocator
                                      : DefaultAllocator::Create()) {
  DEBUG_CHECK(block_size > 0);
}

void Arena::Reset() {
  for (Block& block : blocks_) {
    allocator_->Free(block);
  }
  blocks_.clear();
  cursor_ = nullptr;
  end_ = nullptr;
}

Arena::~Arena() { Reset(); }

uint8_t* Arena::Allocate(size_t bytes, size_t align) {
  DEBUG_CHECK(bytes > 0);
  DEBUG_CHECK((align & (align - 1)) == 0);

  uint8_t* aligned_cursor = reinterpret_cast<uint8_t*>(
      ((reinterpret_cast<uintptr_t>(cursor_) + align - 1) & ~(align - 1)));

  if (aligned_cursor + bytes <= end_) {
    uint8_t* result = aligned_cursor;
    cursor_ = aligned_cursor + bytes;
    return result;
  }

  if (bytes >= block_size_) {
    Block block = allocator_->Alloc(bytes);
    blocks_.push_back(block);
    return block.head;
  }

  Block block = allocator_->Alloc(block_size_);
  blocks_.push_back(block);
  cursor_ = block.head;
  end_ = cursor_ + block_size_;

  DEBUG_CHECK(cursor_ + bytes <= end_);
  uint8_t* result = cursor_;
  cursor_ += bytes;
  return result;
}

void ArenaAllocator::Reset() {
  while (finalizer_head_) {
    finalizer_head_->func(finalizer_head_->ptr);
    finalizer_head_ = finalizer_head_->next;
  }
  finalizer_head_ = nullptr;
  arena_.Reset();
}

ArenaAllocator::~ArenaAllocator() { Reset(); }

}  // namespace skity
