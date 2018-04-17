//
// PROJECT:         Aspia
// FILE:            host/host_session.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session.h"

#include <QDebug>

#include "host/host_session_desktop.h"
#include "host/host_session_file_transfer.h"
#include "host/ipc_channel.h"

namespace aspia {

HostSession::HostSession(const QString& channel_id)
    : channel_id_(channel_id)
{
    // Nothing
}

// static
HostSession* HostSession::create(const QString& session_type, const QString& channel_id)
{
    if (session_type == QLatin1String("desktop_manage"))
    {
        return new HostSessionDesktop(proto::auth::SESSION_TYPE_DESKTOP_MANAGE, channel_id);
    }
    else if (session_type == QLatin1String("desktop_view"))
    {
        return new HostSessionDesktop(proto::auth::SESSION_TYPE_DESKTOP_VIEW, channel_id);
    }
    else if (session_type == QLatin1String("file_transfer"))
    {
        return new HostSessionFileTransfer(channel_id);
    }
    else
    {
        qWarning() << "Unknown session type: " << session_type;
        return nullptr;
    }
}

void HostSession::start()
{
    ipc_channel_ = new IpcChannel(this);

    connect(ipc_channel_, &IpcChannel::connected, this, &HostSession::ipcChannelConnected);
    connect(ipc_channel_, &IpcChannel::disconnected, this, &HostSession::stop);
    connect(ipc_channel_, &IpcChannel::errorOccurred, this, &HostSession::stop);
    connect(ipc_channel_, &IpcChannel::messageWritten, this, &HostSession::messageWritten);
    connect(ipc_channel_, &IpcChannel::messageReceived, [this](const QByteArray& buffer)
    {
        readMessage(buffer);
        ipc_channel_->readMessage();
    });

    connect(this, &HostSession::writeMessage, ipc_channel_, &IpcChannel::writeMessage);
    connect(this, &HostSession::errorOccurred, this, &HostSession::stop);

    ipc_channel_->connectToServer(channel_id_);
}

void HostSession::ipcChannelConnected()
{
    startSession();

    ipc_channel_->readMessage();
}

void HostSession::stop()
{
    delete ipc_channel_;
    stopSession();
}

} // namespace aspia
