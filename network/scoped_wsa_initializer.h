//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/scoped_wsa_initializer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SCOPED_WSA_INITIALIZER_H
#define _ASPIA_NETWORK__SCOPED_WSA_INITIALIZER_H

#include "base/logging.h"
#include "base/macros.h"

#include <winsock2.h>

namespace aspia {

class ScopedWsaInitializer
{
public:
    ScopedWsaInitializer()
    {
        WSADATA data = { 0 };

        // Initialize the socket library.
        is_succeded_ = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
        if (!is_succeded_)
        {
            LOG(ERROR) << "WSAStartup() failed: " << GetLastSystemErrorCodeString();
        }
    }

    ~ScopedWsaInitializer()
    {
        if (is_succeded_)
            WSACleanup();
    }

    bool IsSucceeded() const
    {
        return is_succeded_;
    }

private:
    bool is_succeded_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWsaInitializer);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SCOPED_WSA_INITIALIZER_H
