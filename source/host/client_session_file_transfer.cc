//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/client_session_file_transfer.h"

#include "net/network_channel.h"

namespace host {

ClientSessionFileTransfer::ClientSessionFileTransfer(std::unique_ptr<net::Channel> channel)
    : ClientSession(proto::SESSION_TYPE_FILE_TRANSFER, std::move(channel))
{
    // Nothing
}

ClientSessionFileTransfer::~ClientSessionFileTransfer() = default;

void ClientSessionFileTransfer::onMessageReceived(const base::ByteArray& buffer)
{
    // TODO
}

void ClientSessionFileTransfer::onMessageWritten()
{

}

} // namespace host
