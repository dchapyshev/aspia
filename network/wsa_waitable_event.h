//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/wsa_waitable_event.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__WSA_WAITABLE_EVENT_H
#define _ASPIA_NETWORK__WSA_WAITABLE_EVENT_H

#include "base/macros.h"
#include "base/logging.h"

#include <winsock2.h>
#include <chrono>

namespace aspia {

class WsaWaitableEvent
{
public:
    WsaWaitableEvent() : handle_(WSACreateEvent())
    {
        CHECK(handle_) << GetLastSystemErrorCodeString();
    }

    ~WsaWaitableEvent()
    {
        if (handle_)
        {
            WSACloseEvent(handle_);
            handle_ = nullptr;
        }
    }

    bool Signal()
    {
        return !!WSASetEvent(handle_);
    }

    bool Reset()
    {
        return !!WSAResetEvent(handle_);
    }

    bool Wait()
    {
        return WaitInternal(WSA_INFINITE);
    }

    bool TimedWait(const std::chrono::milliseconds& timeout)
    {
        DCHECK(timeout.count() < std::numeric_limits<DWORD>::max());
        return WaitInternal(static_cast<DWORD>(timeout.count()));
    }

    HANDLE Handle()
    {
        return handle_;
    }

private:
    bool WaitInternal(DWORD timeout)
    {
        if (WSAWaitForMultipleEvents(1, &handle_, TRUE, timeout, TRUE) == WSA_WAIT_FAILED)
        {
            LOG(ERROR) << "WSAWaitForMultipleEvents() failed: "
                       << GetLastSystemErrorCodeString();
            return false;
        }

        return true;
    }

    HANDLE handle_;

    DISALLOW_COPY_AND_ASSIGN(WsaWaitableEvent);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__WSA_WAITABLE_EVENT_H
