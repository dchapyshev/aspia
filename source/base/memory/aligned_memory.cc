//
// PROJECT:         Aspia
// FILE:            base/memory/aligned_memory.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/memory/aligned_memory.h"
#include "base/logging.h"

namespace aspia {

void* AlignedAlloc(size_t size, size_t alignment)
{
    DCHECK_GT(size, 0U);
    DCHECK_EQ(alignment & (alignment - 1), 0U);
    DCHECK_EQ(alignment % sizeof(void*), 0U);

    void* ptr =  _aligned_malloc(size, alignment);

    // Since aligned allocations may fail for non-memory related reasons,
    // force a crash if we encounter a failed allocation; maintaining
    // consistent behavior with a normal allocation failure in Chrome.
    if (!ptr)
    {
        DLOG(LS_ERROR) << "Aligned allocation is incorrect: "
                    << "size=" << size << ", alignment=" << alignment;
        CHECK(false);
    }

    // Sanity check alignment just to be safe.
    DCHECK_EQ(reinterpret_cast<uintptr_t>(ptr) & (alignment - 1), 0U);

    return ptr;
}

}  // namespace aspia
