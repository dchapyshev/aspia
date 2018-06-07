//
// PROJECT:         Aspia
// FILE:            base/aligned_memory.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/aligned_memory.h"

namespace aspia {

void* alignedAlloc(size_t size, size_t alignment)
{
    Q_ASSERT(size > 0U);
    Q_ASSERT((alignment & (alignment - 1)) == 0U);
    Q_ASSERT((alignment % sizeof(void*)) == 0U);

    void* ptr =  qMallocAligned(size, alignment);
    Q_CHECK_PTR(ptr);

    // Sanity check alignment just to be safe.
    Q_ASSERT((reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0U);

    return ptr;
}

}  // namespace aspia
