//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/terminal_agent.h"

#include <QCoreApplication>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "host/terminal_process.h"
#include "proto/terminal.h"

namespace {

const int kDefaultColumns = 80;
const int kDefaultRows = 24;

} // namespace

//--------------------------------------------------------------------------------------------------
TerminalAgent::TerminalAgent(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TerminalAgent::~TerminalAgent()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void TerminalAgent::start(const QString& channel_id)
{
    LOG(INFO) << "Starting (channel_id:" << channel_id << ")";

    ipc_channel_ = new IpcChannel(this);

    connect(ipc_channel_, &IpcChannel::sig_connected, this, &TerminalAgent::onIpcConnected);
    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &TerminalAgent::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_errorOccurred, this, &TerminalAgent::onIpcErrorOccurred);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &TerminalAgent::onIpcMessageReceived);

    ipc_channel_->connectTo(channel_id);
}

//--------------------------------------------------------------------------------------------------
void TerminalAgent::onIpcConnected()
{
    terminal_process_ = TerminalProcess::create(this);
    if (!terminal_process_)
    {
        LOG(ERROR) << "Terminal sessions are not supported on this system";
        QCoreApplication::quit();
        return;
    }

    connect(terminal_process_, &TerminalProcess::sig_output, this, &TerminalAgent::onPtyOutput);
    connect(terminal_process_, &TerminalProcess::sig_finished, this, &TerminalAgent::onPtyFinished);

    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void TerminalAgent::onIpcDisconnected()
{
    LOG(INFO) << "IPC connection is terminated";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void TerminalAgent::onIpcErrorOccurred()
{
    LOG(INFO) << "Unable to connect to IPC server";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void TerminalAgent::onIpcMessageReceived(
    quint32 /* ipc_channel_id */, const QByteArray& buffer, bool /* reliable */)
{
    if (!terminal_process_)
        return;

    proto::terminal::ClientToHost message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse message";
        return;
    }

    if (!process_started_)
    {
        const int columns = message.has_resize() ? message.resize().columns() : kDefaultColumns;
        const int rows = message.has_resize() ? message.resize().rows() : kDefaultRows;

        if (!terminal_process_->start(columns, rows))
        {
            LOG(ERROR) << "Unable to start terminal process";
            QCoreApplication::quit();
            return;
        }

        process_started_ = true;
    }
    else if (message.has_resize())
    {
        terminal_process_->resize(message.resize().columns(), message.resize().rows());
    }

    if (message.has_data())
        terminal_process_->writeInput(QByteArray::fromStdString(message.data().data()));
}

//--------------------------------------------------------------------------------------------------
void TerminalAgent::onPtyOutput(const QByteArray& data)
{
    if (!ipc_channel_)
        return;

    proto::terminal::HostToClient message;
    message.mutable_data()->set_data(std::string(data.constData(), data.size()));
    ipc_channel_->send(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void TerminalAgent::onPtyFinished()
{
    LOG(INFO) << "Terminal process finished";
    QCoreApplication::quit();
}
