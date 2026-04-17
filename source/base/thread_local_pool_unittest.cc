//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/thread_local_pool.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

namespace base {

namespace {

constexpr size_t kBucketSizes[] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192 };
constexpr size_t kBucketCount = std::size(kBucketSizes);
constexpr size_t kMaxFreePerBucket = 32;
constexpr size_t kMaxFree[kBucketCount] = {
    kMaxFreePerBucket, kMaxFreePerBucket, kMaxFreePerBucket, kMaxFreePerBucket,
    kMaxFreePerBucket, kMaxFreePerBucket, kMaxFreePerBucket, kMaxFreePerBucket,
};

using TestPool = ThreadLocalPool<kBucketCount>;

} // namespace

TEST(thread_local_pool_test, allocate_and_deallocate)
{
    TestPool pool(kBucketSizes, kMaxFree);

    void* p = pool.allocate(100);
    ASSERT_NE(p, nullptr);

    // Memory should be writable.
    std::memset(p, 0xAB, 100);

    pool.deallocate(p);
}

TEST(thread_local_pool_test, allocate_zero)
{
    TestPool pool(kBucketSizes, kMaxFree);

    void* p = pool.allocate(0);
    ASSERT_NE(p, nullptr);
    pool.deallocate(p);
}

TEST(thread_local_pool_test, deallocate_null)
{
    TestPool pool(kBucketSizes, kMaxFree);
    pool.deallocate(nullptr); // Should not crash.
}

TEST(thread_local_pool_test, reuse_after_deallocate)
{
    TestPool pool(kBucketSizes, kMaxFree);

    // Allocate and free a block, then allocate again. The pool should reuse the block.
    void* p1 = pool.allocate(50);
    ASSERT_NE(p1, nullptr);
    pool.deallocate(p1);

    void* p2 = pool.allocate(50);
    ASSERT_NE(p2, nullptr);

    // Same bucket, same size -> should get the same block back.
    EXPECT_EQ(p1, p2);
    pool.deallocate(p2);
}

TEST(thread_local_pool_test, different_sizes_different_buckets)
{
    TestPool pool(kBucketSizes, kMaxFree);

    void* p_small = pool.allocate(10);
    void* p_large = pool.allocate(1000);

    ASSERT_NE(p_small, nullptr);
    ASSERT_NE(p_large, nullptr);
    EXPECT_NE(p_small, p_large);

    pool.deallocate(p_small);
    pool.deallocate(p_large);
}

TEST(thread_local_pool_test, oversized_allocation)
{
    TestPool pool(kBucketSizes, kMaxFree);

    // Larger than the biggest bucket — should go directly to malloc.
    void* p = pool.allocate(16000);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xCD, 16000);
    pool.deallocate(p);

    // Oversized blocks are not pooled, so the next allocation should return a different address.
    void* p2 = pool.allocate(16000);
    ASSERT_NE(p2, nullptr);
    pool.deallocate(p2);
}

TEST(thread_local_pool_test, pool_limit_exceeded)
{
    TestPool pool(kBucketSizes, kMaxFree);

    // Allocate more blocks than kMaxFreePerBucket and free them all.
    constexpr size_t kCount = kMaxFreePerBucket + 16;
    std::vector<void*> ptrs(kCount);

    for (size_t i = 0; i < kCount; ++i)
    {
        ptrs[i] = pool.allocate(50);
        ASSERT_NE(ptrs[i], nullptr);
    }

    for (size_t i = 0; i < kCount; ++i)
        pool.deallocate(ptrs[i]);

    // Pool keeps at most kMaxFreePerBucket. Allocate them back to verify no corruption.
    for (size_t i = 0; i < kMaxFreePerBucket; ++i)
    {
        void* p = pool.allocate(50);
        ASSERT_NE(p, nullptr);
        pool.deallocate(p);
    }
}

TEST(thread_local_pool_test, multiple_buckets_interleaved)
{
    TestPool pool(kBucketSizes, kMaxFree);

    void* a1 = pool.allocate(30);   // bucket 64
    void* b1 = pool.allocate(100);  // bucket 128
    void* c1 = pool.allocate(300);  // bucket 512

    pool.deallocate(b1);
    pool.deallocate(a1);
    pool.deallocate(c1);

    // Re-allocate in different order — each should come from its own bucket.
    void* c2 = pool.allocate(300);
    void* a2 = pool.allocate(30);
    void* b2 = pool.allocate(100);

    EXPECT_EQ(a1, a2);
    EXPECT_EQ(b1, b2);
    EXPECT_EQ(c1, c2);

    pool.deallocate(a2);
    pool.deallocate(b2);
    pool.deallocate(c2);
}

TEST(thread_local_pool_test, data_integrity)
{
    TestPool pool(kBucketSizes, kMaxFree);

    constexpr size_t kSize = 200;
    void* p = pool.allocate(kSize);
    ASSERT_NE(p, nullptr);

    // Write a pattern.
    std::memset(p, 0x55, kSize);
    pool.deallocate(p);

    // Re-acquire and write a different pattern.
    void* p2 = pool.allocate(kSize);
    ASSERT_NE(p2, nullptr);
    std::memset(p2, 0xAA, kSize);

    // Verify the pattern.
    auto* bytes = static_cast<unsigned char*>(p2);
    for (size_t i = 0; i < kSize; ++i)
        EXPECT_EQ(bytes[i], 0xAA) << "mismatch at byte " << i;

    pool.deallocate(p2);
}

// ============================================================================
// Benchmark
// ============================================================================

namespace {

struct BenchmarkResult
{
    const char* name;
    size_t iterations;
    double elapsed_ms;
    double ops_per_sec;
};

void printResult(const BenchmarkResult& r)
{
    std::cout << "  " << r.name << ": "
              << r.elapsed_ms << " ms, "
              << static_cast<int64_t>(r.ops_per_sec) << " ops/sec"
              << std::endl;
}

} // namespace

TEST(thread_local_pool_benchmark, malloc_vs_pool)
{
    constexpr size_t kIterations = 500000;
    constexpr size_t kAllocSize = 200;

    std::cout << "\n=== ThreadLocalPool benchmark ===" << std::endl;
    std::cout << "Iterations: " << kIterations
              << ", alloc size: " << kAllocSize << " bytes" << std::endl;

    // --- std::malloc / std::free ---
    {
        auto start = std::chrono::steady_clock::now();

        for (size_t i = 0; i < kIterations; ++i)
        {
            void* p = std::malloc(kAllocSize);
            std::free(p);
        }

        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        BenchmarkResult r;
        r.name = "std::malloc/free";
        r.iterations = kIterations;
        r.elapsed_ms = ms;
        r.ops_per_sec = static_cast<double>(kIterations) / (ms / 1000.0);
        printResult(r);
    }

    // --- ThreadLocalPool ---
    {
        TestPool pool(kBucketSizes, kMaxFree);

        auto start = std::chrono::steady_clock::now();

        for (size_t i = 0; i < kIterations; ++i)
        {
            void* p = pool.allocate(kAllocSize);
            pool.deallocate(p);
        }

        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        BenchmarkResult r;
        r.name = "ThreadLocalPool ";
        r.iterations = kIterations;
        r.elapsed_ms = ms;
        r.ops_per_sec = static_cast<double>(kIterations) / (ms / 1000.0);
        printResult(r);
    }

    std::cout << std::endl;

    // --- Batch: allocate N, then free N ---
    constexpr size_t kBatchSize = 64;
    constexpr size_t kBatchIterations = kIterations / kBatchSize;

    std::cout << "Batch mode: " << kBatchIterations << " batches x "
              << kBatchSize << " allocations, size: " << kAllocSize << " bytes" << std::endl;

    // std::malloc batch
    {
        std::vector<void*> ptrs(kBatchSize);

        auto start = std::chrono::steady_clock::now();

        for (size_t batch = 0; batch < kBatchIterations; ++batch)
        {
            for (size_t i = 0; i < kBatchSize; ++i)
                ptrs[i] = std::malloc(kAllocSize);
            for (size_t i = 0; i < kBatchSize; ++i)
                std::free(ptrs[i]);
        }

        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        BenchmarkResult r;
        r.name = "std::malloc/free (batch)";
        r.iterations = kBatchIterations * kBatchSize;
        r.elapsed_ms = ms;
        r.ops_per_sec = static_cast<double>(r.iterations) / (ms / 1000.0);
        printResult(r);
    }

    // ThreadLocalPool batch
    {
        TestPool pool(kBucketSizes, kMaxFree);
        std::vector<void*> ptrs(kBatchSize);

        auto start = std::chrono::steady_clock::now();

        for (size_t batch = 0; batch < kBatchIterations; ++batch)
        {
            for (size_t i = 0; i < kBatchSize; ++i)
                ptrs[i] = pool.allocate(kAllocSize);
            for (size_t i = 0; i < kBatchSize; ++i)
                pool.deallocate(ptrs[i]);
        }

        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        BenchmarkResult r;
        r.name = "ThreadLocalPool  (batch)";
        r.iterations = kBatchIterations * kBatchSize;
        r.elapsed_ms = ms;
        r.ops_per_sec = static_cast<double>(r.iterations) / (ms / 1000.0);
        printResult(r);
    }

    std::cout << std::endl;

    // --- Mixed sizes ---
    constexpr size_t kMixedSizes[] = { 16, 50, 100, 200, 500, 1000, 4000 };
    constexpr size_t kMixedCount = std::size(kMixedSizes);
    constexpr size_t kMixedIterations = kIterations / kMixedCount;

    std::cout << "Mixed sizes: " << kMixedIterations << " rounds x "
              << kMixedCount << " sizes" << std::endl;

    // std::malloc mixed
    {
        auto start = std::chrono::steady_clock::now();

        for (size_t round = 0; round < kMixedIterations; ++round)
        {
            for (size_t s = 0; s < kMixedCount; ++s)
            {
                void* p = std::malloc(kMixedSizes[s]);
                std::free(p);
            }
        }

        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        BenchmarkResult r;
        r.name = "std::malloc/free (mixed)";
        r.iterations = kMixedIterations * kMixedCount;
        r.elapsed_ms = ms;
        r.ops_per_sec = static_cast<double>(r.iterations) / (ms / 1000.0);
        printResult(r);
    }

    // ThreadLocalPool mixed
    {
        TestPool pool(kBucketSizes, kMaxFree);

        auto start = std::chrono::steady_clock::now();

        for (size_t round = 0; round < kMixedIterations; ++round)
        {
            for (size_t s = 0; s < kMixedCount; ++s)
            {
                void* p = pool.allocate(kMixedSizes[s]);
                pool.deallocate(p);
            }
        }

        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        BenchmarkResult r;
        r.name = "ThreadLocalPool  (mixed)";
        r.iterations = kMixedIterations * kMixedCount;
        r.elapsed_ms = ms;
        r.ops_per_sec = static_cast<double>(r.iterations) / (ms / 1000.0);
        printResult(r);
    }

    std::cout << std::endl;
}

} // namespace base
