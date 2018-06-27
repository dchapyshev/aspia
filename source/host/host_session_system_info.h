//
// PROJECT:         Aspia
// FILE:            host/host_session_system_info.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H
#define _ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H

#include "host/host_session.h"

namespace aspia {

class HostSessionSystemInfo : public HostSession
{
    Q_OBJECT

public:
    explicit HostSessionSystemInfo(const QString& channel_id);
    ~HostSessionSystemInfo() = default;

public slots:
    // HostSession implementation.
    void messageReceived(const QByteArray& buffer) override;
    void messageWritten(int message_id) override;

protected:
    void startSession() override;
    void stopSession() override;

private:
    Q_DISABLE_COPY(HostSessionSystemInfo)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H
