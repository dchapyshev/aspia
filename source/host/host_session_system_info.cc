//
// PROJECT:         Aspia
// FILE:            host/host_session_system_info.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_system_info.h"

#include "base/message_serialization.h"
#include "protocol/system_info_session.pb.h"

namespace aspia {

namespace {

enum MessageId { ReplyMessageId };

} // namespace

HostSessionSystemInfo::HostSessionSystemInfo(const QString& channel_id)
    : HostSession(channel_id)
{
    // Nothing
}

void HostSessionSystemInfo::messageReceived(const QByteArray& buffer)
{
    proto::system_info::Request request;

    if (!parseMessage(buffer, request))
    {
        emit errorOccurred();
        return;
    }

    // TODO
}

void HostSessionSystemInfo::messageWritten(int message_id)
{
    Q_ASSERT(message_id == ReplyMessageId);
    emit readMessage();
}

void HostSessionSystemInfo::startSession()
{
    // TODO
}

void HostSessionSystemInfo::stopSession()
{
    // TODO
}

} // namespace aspia
