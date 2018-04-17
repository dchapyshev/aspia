//
// PROJECT:         Aspia
// FILE:            host/host_session_file_transfer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_file_transfer.h"

#include <QDebug>

#include "base/message_serialization.h"
#include "host/file_worker.h"

namespace aspia {

HostSessionFileTransfer::HostSessionFileTransfer(const QString& channel_id)
    : HostSession(channel_id)
{
    // Nothing
}

void HostSessionFileTransfer::startSession()
{
    worker_ = new FileWorker(this);
}

void HostSessionFileTransfer::stopSession()
{
    delete worker_;
}

void HostSessionFileTransfer::readMessage(const QByteArray& buffer)
{
    if (worker_.isNull())
        return;

    proto::file_transfer::Request request;

    if (!parseMessage(buffer, request))
    {
        emit errorOccurred();
        return;
    }

    emit writeMessage(-1, serializeMessage(worker_->doRequest(request)));
}

void HostSessionFileTransfer::messageWritten(int /* message_id */)
{
    // Nothing
}

} // namespace aspia
