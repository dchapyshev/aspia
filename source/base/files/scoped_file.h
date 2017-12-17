//
// PROJECT:         Aspia
// FILE:            base/files/scoped_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__SCOPED_FILE_H
#define _ASPIA_BASE__FILES__SCOPED_FILE_H

#include <stdio.h>
#include <memory>

namespace aspia {

namespace internal {

// Functor for |ScopedFILE| (below).
struct ScopedFILECloser
{
    inline void operator()(FILE* x) const
    {
        if (x)
            fclose(x);
    }
};

}  // namespace internal

// -----------------------------------------------------------------------------

// Automatically closes |FILE*|s.
using ScopedFILE = std::unique_ptr<FILE, internal::ScopedFILECloser>;

} // namespace aspia

#endif // _ASPIA_BASE__FILES__SCOPED_FILE_H
