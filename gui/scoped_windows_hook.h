//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/scoped_windows_hook.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__SCOPED_WINDOWS_HOOK_H
#define _ASPIA_GUI__SCOPED_WINDOWS_HOOK_H

#include "aspia_config.h"

#include "base/logging.h"

namespace aspia {

class ScopedWindowsHook
{
public:
    ScopedWindowsHook() : hook_(nullptr)
    {
        // Nothing
    }

    ~ScopedWindowsHook()
    {
        Reset();
    }

    void Set(int hook_id, HOOKPROC proc)
    {
        if (!hook_)
        {
            hook_ = SetWindowsHookExW(hook_id,
                                      proc,
                                      GetModuleHandleW(nullptr),
                                      0);
            if (!hook_)
            {
                LOG(WARNING) << "SetWindowsHookExW() failed: " << GetLastError();
            }
        }
    }

    void Reset()
    {
        if (hook_)
        {
            UnhookWindowsHookEx(hook_);
            hook_ = nullptr;
        }
    }

private:
    HHOOK hook_;
};

} // namespace aspia

#endif // _ASPIA_GUI__SCOPED_WINDOWS_HOOK_H
