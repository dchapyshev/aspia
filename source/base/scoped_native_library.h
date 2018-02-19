//
// PROJECT:         Aspia
// FILE:            base/scoped_native_library.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_NATIVE_LIBRARY_H
#define _ASPIA_BASE__SCOPED_NATIVE_LIBRARY_H

#include "base/macros.h"

namespace aspia {

class ScopedNativeLibrary
{
public:
    explicit ScopedNativeLibrary(const wchar_t* lib_name)
    {
        lib_ = LoadLibraryW(lib_name);
    }

    virtual ~ScopedNativeLibrary()
    {
        if (IsLoaded())
            FreeLibrary(lib_);
    }

    void* GetFunctionPointer(const char* function_name) const
    {
        return GetProcAddress(lib_, function_name);
    }

    bool IsLoaded() const { return lib_ != nullptr; }

private:
    HMODULE lib_;

    DISALLOW_COPY_AND_ASSIGN(ScopedNativeLibrary);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_NATIVE_LIBRARY_H
