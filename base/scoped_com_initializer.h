//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_com_initializer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_COM_INITIALIZER_H
#define _ASPIA_BASE__SCOPED_COM_INITIALIZER_H

#include <objbase.h>

#include "base/logging.h"
#include "base/macros.h"

namespace aspia {

// Initializes COM in the constructor (STA or MTA), and uninitializes COM in the
// destructor.
//
// WARNING: This should only be used once per thread, ideally scoped to a
// similar lifetime as the thread itself.  You should not be using this in
// random utility functions that make COM calls -- instead ensure these
// functions are running on a COM-supporting thread!
class ScopedCOMInitializer
{
public:
    // Enum value provided to initialize the thread as an MTA instead of STA.
    enum SelectMTA { kMTA };

    // Constructor for STA initialization.
    ScopedCOMInitializer()
    {
        Initialize(COINIT_APARTMENTTHREADED);
    }

    // Constructor for MTA initialization.
    explicit ScopedCOMInitializer(SelectMTA mta)
    {
        UNREF(mta);
        Initialize(COINIT_MULTITHREADED);
    }

    ~ScopedCOMInitializer()
    {
#ifndef NDEBUG
        // Using the windows API directly to avoid dependency on platform_thread.
        DCHECK_EQ(GetCurrentThreadId(), thread_id_);
#endif

        if (IsSucceeded())
            CoUninitialize();
    }

    bool IsSucceeded() const { return SUCCEEDED(hr_); }

private:
    void Initialize(COINIT init)
    {
#ifndef NDEBUG
        thread_id_ = GetCurrentThreadId();
#endif
        hr_ = CoInitializeEx(NULL, init);
#ifndef NDEBUG
        if (hr_ == S_FALSE)
            LOG(ERROR) << "Multiple CoInitialize() calls for thread " << thread_id_;
        else
            DCHECK_NE(RPC_E_CHANGED_MODE, hr_) << "Invalid COM thread model change";
#endif
    }

    HRESULT hr_;
#ifndef NDEBUG
  // In debug builds we use this variable to catch a potential bug where a
  // ScopedCOMInitializer instance is deleted on a different thread than it
  // was initially created on.  If that ever happens it can have bad
  // consequences and the cause can be tricky to track down.
    DWORD thread_id_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};

}  // namespace aspia

#endif  // _ASPIA_BASE__SCOPED_COM_INITIALIZER_H
