//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/aligned_memory.h"

#include <gtest/gtest.h>

namespace base {

#define EXPECT_ALIGNED(ptr, align) \
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(ptr) & ((align) - 1))

TEST(aligned_memory_test, dynamic_allocation)
{
    void* p = alignedAlloc(8, 8);
    EXPECT_TRUE(p);
    EXPECT_ALIGNED(p, 8);
    alignedFree(p);

    p = alignedAlloc(8, 16);
    EXPECT_TRUE(p);
    EXPECT_ALIGNED(p, 16);
    alignedFree(p);

    p = alignedAlloc(8, 256);
    EXPECT_TRUE(p);
    EXPECT_ALIGNED(p, 256);
    alignedFree(p);

    p = alignedAlloc(8, 4096);
    EXPECT_TRUE(p);
    EXPECT_ALIGNED(p, 4096);
    alignedFree(p);
}

TEST(aligned_memory_test, scoped_dynamic_allocation)
{
    std::unique_ptr<float, AlignedFreeDeleter> p(static_cast<float*>(alignedAlloc(8, 8)));
    EXPECT_TRUE(p.get());
    EXPECT_ALIGNED(p.get(), 8);
}

} // namespace base
