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

#ifndef BASE_BUCKET_POOL_H
#define BASE_BUCKET_POOL_H

#include <array>
#include <cstdlib>

namespace base {

// Bucket descriptor for BucketPool. Pairs up a bucket size with its cache cap. |size| is the
// maximum total allocation (including internal header) for the bucket. |max_free| is the maximum
// cached blocks before releasing to system.
//
// Declared outside the pool template so that the same type is used across all instantiations
// (allowing std::size() to deduce BucketCount from the array passed to the constructor).
struct Bucket
{
    size_t size;
    size_t max_free;
};

// Bucket-based memory pool. Not synchronized - wrap the instance in `thread_local` or otherwise
// guarantee single-threaded access. Allocations are bucketed by size class; oversized allocations
// fall through to std::malloc.
//
// Usage:
//   constexpr Bucket kBuckets[] = {
//       { 64,   32 },
//       { 128,  16 },
//       { 4096,  4 },
//   };
//   using MyPool = BucketPool<std::size(kBuckets)>;
//   thread_local MyPool pool(kBuckets);
//   void* p = pool.allocate(100);  // returns block from 128-byte bucket
//   pool.deallocate(p);            // returns block to pool for reuse
//
// Template parameters:
//   BucketCount - number of size classes
//
template <size_t BucketCount>
class BucketPool
{
public:
    // |buckets| must be sorted by |size| in ascending order.
    explicit BucketPool(const Bucket (&buckets)[BucketCount])
    {
        for (size_t i = 0; i < BucketCount; ++i)
            buckets_[i] = buckets[i];
    }

    ~BucketPool()
    {
        for (size_t i = 0; i < BucketCount; ++i)
        {
            FreeBlock* block = free_lists_[i];
            while (block)
            {
                FreeBlock* next = block->next;
                std::free(block);
                block = next;
            }
        }
    }

    void* allocate(size_t user_size)
    {
        const size_t total = sizeof(AllocHeader) + user_size;
        const size_t bucket = findBucket(total);

        void* raw = nullptr;

        if (bucket != kNoBucket && free_lists_[bucket])
        {
            FreeBlock* block = free_lists_[bucket];
            free_lists_[bucket] = block->next;
            --free_counts_[bucket];
            raw = block;
        }
        else
        {
            const size_t alloc_size = (bucket != kNoBucket) ? buckets_[bucket].size : total;
            raw = std::malloc(alloc_size);
            if (!raw)
                return nullptr;
        }

        auto* header = static_cast<AllocHeader*>(raw);
        header->alloc_size = (bucket != kNoBucket) ? buckets_[bucket].size : total;
        header->bucket = bucket;

        return header + 1;
    }

    void deallocate(void* ptr)
    {
        if (!ptr)
            return;

        auto* header = reinterpret_cast<AllocHeader*>(
            static_cast<char*>(ptr) - sizeof(AllocHeader));
        const size_t bucket = header->bucket;

        if (bucket != kNoBucket && free_counts_[bucket] < buckets_[bucket].max_free)
        {
            auto* block = reinterpret_cast<FreeBlock*>(header);
            block->next = free_lists_[bucket];
            free_lists_[bucket] = block;
            ++free_counts_[bucket];
        }
        else
        {
            std::free(header);
        }
    }

    BucketPool(const BucketPool&) = delete;
    BucketPool& operator=(const BucketPool&) = delete;

private:
    static constexpr size_t kNoBucket = static_cast<size_t>(-1);

    struct AllocHeader
    {
        size_t alloc_size;
        size_t bucket;
    };

    struct FreeBlock
    {
        FreeBlock* next;
    };

    size_t findBucket(size_t total_size) const
    {
        for (size_t i = 0; i < BucketCount; ++i)
        {
            if (total_size <= buckets_[i].size)
                return i;
        }
        return kNoBucket;
    }

    std::array<Bucket, BucketCount> buckets_ = {};
    std::array<FreeBlock*, BucketCount> free_lists_ = {};
    std::array<size_t, BucketCount> free_counts_ = {};
};

} // namespace base

#endif // BASE_BUCKET_POOL_H
