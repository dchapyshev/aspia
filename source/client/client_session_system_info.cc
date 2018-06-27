//
// PROJECT:         Aspia
// FILE:            client/client_session_system_info.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_system_info.h"

#include "base/message_serialization.h"
#include "client/ui/system_info_window.h"

namespace aspia {

namespace {

enum MessageId { RequestMessageId };

} // namespace

ClientSessionSystemInfo::ClientSessionSystemInfo(ConnectData* connect_data, QObject* parent)
    : ClientSession(parent),
      connect_data_(connect_data)
{
    qRegisterMetaType<proto::system_info::Request>();
    qRegisterMetaType<proto::system_info::Reply>();
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    delete window_;
}

void ClientSessionSystemInfo::messageReceived(const QByteArray& buffer)
{
    proto::system_info::Reply reply;

    if (!parseMessage(buffer, reply))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    // TODO
}

void ClientSessionSystemInfo::messageWritten(int message_id)
{
    Q_ASSERT(message_id == RequestMessageId);
    emit readMessage();
}

void ClientSessionSystemInfo::startSession()
{
    window_ = new SystemInfoWindow(connect_data_);

    // When the window is closed, we close the session.
    connect(window_, &SystemInfoWindow::windowClose,
            this, &ClientSessionSystemInfo::closedByUser);

    window_->show();
    window_->activateWindow();
}

void ClientSessionSystemInfo::closeSession()
{
    // If the end of the session is not initiated by the user, then we do not send the session
    // end signal.
    disconnect(window_, &SystemInfoWindow::windowClose,
               this, &ClientSessionSystemInfo::closedByUser);
    window_->close();
}

} // namespace aspia
