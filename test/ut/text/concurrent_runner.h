// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

/**
 * Utility class to run a task concurrently in multiple threads.
 *
 * - All threads start execution at the same time (barrier semantics).
 * - Optional scheduling perturbation to increase race detection probability.
 * - Suitable for TSAN-based race detection.
 */
class ConcurrentRunner {
 public:
  ConcurrentRunner(int thread_count, int iterations)
      : thread_count_(thread_count), iterations_(iterations) {}

  template <typename Fn>
  void Run(Fn fn) {
    std::mutex mtx;
    std::condition_variable cv;
    int ready = 0;
    bool start = false;

    std::vector<std::thread> threads;
    threads.reserve(thread_count_);

    for (int t = 0; t < thread_count_; ++t) {
      auto task = fn;

      threads.emplace_back([task = std::move(task), &mtx, &cv, &ready, &start,
                            task_count = thread_count_,
                            iterations = iterations_]() mutable {
        {
          std::unique_lock<std::mutex> lock(mtx);
          ready++;
          if (ready == task_count) {
            start = true;
            cv.notify_all();
          } else {
            cv.wait(lock, [&]() { return start; });
          }
        }

        for (int i = 0; i < iterations; ++i) {
          task(i);  // 每个线程独立调用自己的 task
          FuzzYield();
        }
      });
    }

    for (auto& th : threads) {
      th.join();
    }
  }

 private:
  static void FuzzYield() {
    // Introduce scheduling noise to amplify race conditions
    static thread_local std::mt19937 rng{std::random_device{}()};
    if ((rng() & 0x3) == 0) {
      std::this_thread::yield();
    }
  }

  int thread_count_;
  int iterations_;
};
