//
// PROJECT:         Aspia
// FILE:            host/host_session_fake_file_transfer.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_fake_file_transfer.h"

#include "base/message_serialization.h"
#include "protocol/file_transfer_session.pb.h"

namespace aspia {

namespace {

enum MessageId { ReplyMessage = 1000 };

} // namespace

HostSessionFakeFileTransfer::HostSessionFakeFileTransfer(QObject* parent)
    : HostSessionFake(parent)
{
    // Nothing
}

void HostSessionFakeFileTransfer::startSession()
{
    emit readMessage();
}

void HostSessionFakeFileTransfer::onMessageReceived(const QByteArray& /* buffer */)
{
    proto::file_transfer::Reply reply;
    reply.set_status(proto::file_transfer::STATUS_NO_LOGGED_ON_USER);
    emit writeMessage(ReplyMessage, serializeMessage(reply));
}

void HostSessionFakeFileTransfer::onMessageWritten(int message_id)
{
    if (message_id == ReplyMessage)
        emit errorOccurred();
}

} // namespace aspia
