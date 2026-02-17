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

// AlignedMemory is a POD type that gives you a portable way to specify static
// or local stack data of a given alignment and size. For example, if you need
// static storage for a class, but you want manual control over when the object
// is constructed and destructed (you don't want static initialization and
// destruction), use AlignedMemory:
//
//   static AlignedMemory<sizeof(MyClass), ALIGNOF(MyClass)> my_class;
//
//   // ... at runtime:
//   new(my_class.void_data()) MyClass();
//
//   // ... use it:
//   MyClass* mc = my_class.data_as<MyClass>();
//
//   // ... later, to destruct my_class:
//   my_class.data_as<MyClass>()->MyClass::~MyClass();
//
// Alternatively, a runtime sized aligned allocation can be created:
//
//   float* my_array = static_cast<float*>(alignedAlloc(size, alignment));
//
//   // ... later, to release the memory:
//   alignedFree(my_array);
//
// Or using unique_ptr:
//
//   std::unique_ptr<float, AlignedFreeDeleter> my_array(
//       static_cast<float*>(alignedAlloc(size, alignment)));

#ifndef BASE_ALIGNED_MEMORY_H
#define BASE_ALIGNED_MEMORY_H

#include <QtGlobal>
#include <cstddef>

#if defined(Q_OS_WINDOWS)
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace base {

void* alignedAlloc(size_t size, size_t alignment);

inline void alignedFree(void* ptr)
{
#if defined(Q_OS_WINDOWS)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// Deleter for use with unique_ptr. E.g., use as std::unique_ptr<Foo, AlignedFreeDeleter> foo;
struct AlignedFreeDeleter
{
    void operator()(void* ptr) const
    {
        alignedFree(ptr);
    }
};

}  // namespace base

#endif // BASE_ALIGNED_MEMORY_H
