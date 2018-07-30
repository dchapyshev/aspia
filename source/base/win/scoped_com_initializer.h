//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__WIN__SCOPED_COM_INITIALIZER_H_
#define ASPIA_BASE__WIN__SCOPED_COM_INITIALIZER_H_

#include <objbase.h>

#ifndef NDEBUG
#include <QDebug>
#endif

#include "base/macros_magic.h"

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
        initialize(COINIT_APARTMENTTHREADED);
    }

    // Constructor for MTA initialization.
    explicit ScopedCOMInitializer(SelectMTA /* mta */)
    {
        initialize(COINIT_MULTITHREADED);
    }

    ~ScopedCOMInitializer()
    {
#ifndef NDEBUG
        // Using the windows API directly to avoid dependency on platform_thread.
        Q_ASSERT(GetCurrentThreadId() == thread_id_);
#endif

        if (isSucceeded())
            CoUninitialize();
    }

    bool isSucceeded() const { return SUCCEEDED(hr_); }

private:
    void initialize(COINIT init)
    {
#ifndef NDEBUG
        thread_id_ = GetCurrentThreadId();
#endif
        hr_ = CoInitializeEx(nullptr, init);
#ifndef NDEBUG
        if (hr_ == S_FALSE)
        {
            qWarning() << "Multiple CoInitialize() calls for thread " << thread_id_;
        }
        else
        {
            if (hr_ != RPC_E_CHANGED_MODE)
                qFatal("Invalid COM thread model change");
        }
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

#endif // ASPIA_BASE__WIN__SCOPED_COM_INITIALIZER_H_
