//
// PROJECT:         Aspia
// FILE:            client/client_session_system_info.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_system_info.h"

#include "base/message_serialization.h"

namespace aspia {

namespace {

enum MessageId { RequestMessageId };

} // namespace

ClientSessionSystemInfo::ClientSessionSystemInfo(ConnectData* connect_data, QObject* parent)
    : ClientSession(parent),
      connect_data_(connect_data)
{
    // Nothing
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    // TODO
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
    // TODO
}

void ClientSessionSystemInfo::closeSession()
{
    // TODO
}

} // namespace aspia
