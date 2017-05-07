//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/status.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__STATUS_H
#define _ASPIA_CLIENT__STATUS_H

namespace aspia {

enum class ClientStatus
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    CONNECT_TIMEOUT,
    CONNECT_ERROR,
    INVALID_USERNAME_OR_PASSWORD,
    NOT_SUPPORTED_AUTH_METHOD,
    SESSION_TYPE_NOT_ALLOWED
};

} // namespace aspia

#endif // _ASPIA_CLIENT__STATUS_H
