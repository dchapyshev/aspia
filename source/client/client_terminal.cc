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

#include "client/client_terminal.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "proto/peer.h"
#include "proto/terminal.h"

//--------------------------------------------------------------------------------------------------
ClientTerminal::ClientTerminal(QObject* parent)
    : Client(parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientTerminal::~ClientTerminal()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientTerminal::sendCredentials(const QString& user_name, const QString& password)
{
    proto::terminal::ClientToHost message;
    proto::terminal::Credentials* credentials = message.mutable_credentials();
    credentials->set_user_name(user_name.toStdString());
    credentials->set_password(password.toStdString());
    sendMessage(proto::peer::CHANNEL_ID_0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientTerminal::sendInput(const QByteArray& data)
{
    proto::terminal::ClientToHost message;
    message.mutable_data()->set_data(std::string(data.constData(), data.size()));
    sendMessage(proto::peer::CHANNEL_ID_0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientTerminal::sendResize(int columns, int rows)
{
    proto::terminal::ClientToHost message;
    proto::terminal::Resize* resize = message.mutable_resize();
    resize->set_columns(columns);
    resize->set_rows(rows);
    sendMessage(proto::peer::CHANNEL_ID_0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientTerminal::onStarted()
{
    CLOG(INFO) << "Terminal session started";
    emit sig_showSessionWindow();
}

//--------------------------------------------------------------------------------------------------
void ClientTerminal::onMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::terminal::HostToClient message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Unable to parse message";
        return;
    }

    if (message.has_data())
        emit sig_outputReceived(QByteArray::fromStdString(message.data().data()));

    if (message.has_result())
        emit sig_resultReceived(message.result().code());
}
