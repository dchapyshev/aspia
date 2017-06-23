//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/memory/aligned_memory.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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
//   float* my_array = static_cast<float*>(AlignedAlloc(size, alignment));
//
//   // ... later, to release the memory:
//   AlignedFree(my_array);
//
// Or using unique_ptr:
//
//   std::unique_ptr<float, AlignedFreeDeleter> my_array(
//       static_cast<float*>(AlignedAlloc(size, alignment)));

#ifndef _ASPIA_BASE__MEMORY__ALIGNED_MEMORY_H
#define _ASPIA_BASE__MEMORY__ALIGNED_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <malloc.h>

namespace aspia {

void* AlignedAlloc(size_t size, size_t alignment);

inline void AlignedFree(void* ptr)
{
    _aligned_free(ptr);
}

// Deleter for use with unique_ptr. E.g., use as
//   std::unique_ptr<Foo, base::AlignedFreeDeleter> foo;
struct AlignedFreeDeleter
{
    inline void operator()(void* ptr) const
    {
        AlignedFree(ptr);
    }
};

}  // namespace aspia

#endif  // _ASPIA_BASE__MEMORY__ALIGNED_MEMORY_H
