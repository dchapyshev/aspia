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

#include "host/workers/user_ipc_worker.h"

#include <gtest/gtest.h>

#include <condition_variable>
#include <functional>
#include <mutex>

// Drives the worker from its own thread, as the IPC channel to the service does.
class UserIpcWorkerTestPeer
{
public:
    explicit UserIpcWorkerTestPeer(UserIpcWorker* worker)
        : worker_(worker)
    {
        // Nothing
    }

    void connectClient(quint32 client_id)
    {
        invoke([&]()
        {
            proto::user::ConnectEvent event;
            event.set_client_id(client_id);
            event.set_session_type(proto::peer::SESSION_TYPE_DESKTOP);
            worker_->onConnectEvent(event);
        });
    }

    void loseService()
    {
        invoke([&]() { worker_->onIpcDisconnected(); });
    }

private:
    // Runs |work| in the worker thread and returns once it has finished.
    void invoke(const std::function<void()>& work)
    {
        std::mutex lock;
        std::condition_variable finished;
        bool done = false;

        worker_->post([&]()
        {
            work();

            std::lock_guard guard(lock);
            done = true;
            finished.notify_one();
        });

        std::unique_lock guard(lock);
        finished.wait(guard, [&]() { return done; });
    }

    UserIpcWorker* worker_;
};

//--------------------------------------------------------------------------------------------------
// The service restarted while a client was connected, and the GUI outlived it. The clients of the
// previous connection are gone with the service, so the list after the reconnect holds only the new
// client.
TEST(user_ipc_worker_test, clients_of_lost_service_are_forgotten)
{
    WorkerManager workers;

    auto owned_worker = std::make_unique<UserIpcWorker>();
    UserIpcWorker* worker = owned_worker.get();
    workers.add(std::move(owned_worker));
    workers.start();

    UserIpcWorker::ClientList last_list;
    QObject::connect(worker, &UserIpcWorker::sig_clientListChanged, worker,
                     [&](const UserIpcWorker::ClientList& clients) { last_list = clients; },
                     Qt::DirectConnection);

    UserIpcWorkerTestPeer peer(worker);

    peer.connectClient(1);
    ASSERT_EQ(last_list.size(), 1);

    peer.loseService();
    peer.connectClient(2);

    ASSERT_EQ(last_list.size(), 1);
    EXPECT_EQ(last_list.front().client_id, 2u);
}
