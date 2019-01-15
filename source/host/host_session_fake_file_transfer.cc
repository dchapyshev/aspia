//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_session_fake_file_transfer.h"

#include "common/message_serialization.h"
#include "proto/file_transfer_session.pb.h"

namespace host {

SessionFakeFileTransfer::SessionFakeFileTransfer(QObject* parent)
    : SessionFake(parent)
{
    // Nothing
}

void SessionFakeFileTransfer::startSession()
{
    proto::file_transfer::Reply reply;
    reply.set_status(proto::file_transfer::STATUS_NO_LOGGED_ON_USER);
    emit sendMessage(common::serializeMessage(reply));
}

void SessionFakeFileTransfer::onMessageReceived(const QByteArray& /* buffer */)
{
    proto::file_transfer::Reply reply;
    reply.set_status(proto::file_transfer::STATUS_NO_LOGGED_ON_USER);
    emit sendMessage(common::serializeMessage(reply));
}

} // namespace host
