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

#include "client/client_pool.h"

#include <QObject>

#include "client/ui/client_dialog.h"
#include "client/client.h"

namespace aspia {

namespace {

class ClientPoolImpl;

std::unique_ptr<ClientPoolImpl> g_pool;
std::mutex g_pool_lock;

class ClientPoolImpl : public QObject
{
public:
    ClientPoolImpl() = default;
    ~ClientPoolImpl() = default;

    void connectWith(const ConnectData& connect_data);

private slots:
    void onClientFinished(Client* client);

private:
    using ScopedClient = std::unique_ptr<Client>;
    using ClientList = std::list<ScopedClient>;

    ClientList client_list_;

    DISALLOW_COPY_AND_ASSIGN(ClientPoolImpl);
};

void ClientPoolImpl::connectWith(const ConnectData& connect_data)
{
    Client* client = new Client(connect_data, this);
    connect(client, &Client::finished, this, &ClientPoolImpl::onClientFinished);
    client_list_.emplace_back(client);

    client->start();
}

void ClientPoolImpl::onClientFinished(Client* client)
{
    for (auto it = client_list_.begin(); it != client_list_.end(); ++it)
    {
        if (client == it->get())
        {
            client_list_.erase(it);
            break;
        }
    }
}

} // namespace

// static
void ClientPool::connect(const ConnectData& connect_data)
{
    std::scoped_lock lock(g_pool_lock);
    if (!g_pool)
        g_pool = std::make_unique<ClientPoolImpl>();

    g_pool->connectWith(connect_data);
}

// static
void ClientPool::connect()
{
    ClientDialog dialog;
    if (dialog.exec() != ClientDialog::Accepted)
        return;

    connect(dialog.connectData());
}

} // namespace aspia
