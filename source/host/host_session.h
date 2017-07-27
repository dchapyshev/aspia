//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_H
#define _ASPIA_HOST__HOST_SESSION_H

namespace aspia {

class HostSession
{
public:
    HostSession() = default;
    virtual ~HostSession() = default;
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_H
