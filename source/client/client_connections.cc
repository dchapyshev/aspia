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

#include "client/client_connections.h"

#include "client/client.h"

namespace aspia {

ClientConnections::ClientConnections(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

void ClientConnections::connectWith(const ConnectData& connect_data)
{
    Client* client = new Client(connect_data, this);
    connect(client, &Client::finished, this, &ClientConnections::onClientFinished);
    client_list_.push_back(client);

    client->start();
}

void ClientConnections::onClientFinished(Client* client)
{
    for (auto it = client_list_.begin(); it != client_list_.end(); ++it)
    {
        if (client == *it)
        {
            client_list_.erase(it);
            break;
        }
    }

    delete client;
}

} // namespace aspia
