//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_status.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_STATUS_H
#define _ASPIA_CLIENT__CLIENT_STATUS_H

namespace aspia {

enum class ClientStatus
{
    INVALID_HOSTNAME,
    INVALID_PORT,
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

#endif // _ASPIA_CLIENT__CLIENT_STATUS_H
