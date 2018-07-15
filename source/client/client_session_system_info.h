//
// PROJECT:         Aspia
// FILE:            client/client_session_system_info.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H
#define _ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H

#include "client/client_session.h"
#include "client/connect_data.h"
#include "host/system_info_request.h"
#include "protocol/system_info_session.pb.h"

namespace aspia {

Q_DECLARE_METATYPE(proto::system_info::Request);
Q_DECLARE_METATYPE(proto::system_info::Reply);

class SystemInfoWindow;

class ClientSessionSystemInfo : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionSystemInfo(ConnectData* connect_data, QObject* parent);
    ~ClientSessionSystemInfo() = default;

public slots:
    // ClientSession implementation.
    void messageReceived(const QByteArray& buffer) override;
    void messageWritten(int message_id) override;
    void startSession() override;
    void closeSession() override;

private slots:
    void onNewRequest(SystemInfoRequest* request);

private:
    void writeNextRequest();

    ConnectData* connect_data_;

    std::unique_ptr<SystemInfoWindow> window_;
    std::list<std::unique_ptr<SystemInfoRequest>> requests_;

    Q_DISABLE_COPY(ClientSessionSystemInfo)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_SYSTEM_INFO_H
