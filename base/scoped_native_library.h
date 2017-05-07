//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_native_library.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_NATIVE_LIBRARY_H
#define _ASPIA_BASE__SCOPED_NATIVE_LIBRARY_H

#include "base/macros.h"

#include <string>

namespace aspia {

class ScopedNativeLibrary
{
public:
    explicit ScopedNativeLibrary(const WCHAR* lib_name)
    {
        lib_ = LoadLibraryW(lib_name);
    }

    virtual ~ScopedNativeLibrary()
    {
        if (lib_)
            FreeLibrary(lib_);
    }

    void* GetFunctionPointer(const char* function_name) const
    {
        return GetProcAddress(lib_, function_name);
    }

private:
    HMODULE lib_;

    DISALLOW_COPY_AND_ASSIGN(ScopedNativeLibrary);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_NATIVE_LIBRARY_H
