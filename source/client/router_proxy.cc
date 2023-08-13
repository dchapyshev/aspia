//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/router_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/router.h"

namespace client {

class RouterProxy::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<base::TaskRunner> io_task_runner, std::unique_ptr<Router> router);
    ~Impl();

    void connectToRouter(const std::u16string& address, uint16_t port);
    void disconnectFromRouter();
    void refreshSessionList();
    void stopSession(int64_t session_id);
    void refreshUserList();
    void addUser(const proto::User& user);
    void modifyUser(const proto::User& user);
    void deleteUser(int64_t entry_id);
    void disconnectPeerSession(int64_t relay_session_id, uint64_t peer_session_id);

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::unique_ptr<Router> router_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
RouterProxy::Impl::Impl(std::shared_ptr<base::TaskRunner> io_task_runner,
                        std::unique_ptr<Router> router)
    : io_task_runner_(std::move(io_task_runner)),
      router_(std::move(router))
{
    DCHECK(io_task_runner_ && router_);
}

//--------------------------------------------------------------------------------------------------
RouterProxy::Impl::~Impl()
{
    DCHECK(!router_);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::connectToRouter(const std::u16string& address, uint16_t port)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&Impl::connectToRouter, shared_from_this(), address, port));
        return;
    }

    if (router_)
        router_->connectToRouter(address, port);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::disconnectFromRouter()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&Impl::disconnectFromRouter, shared_from_this()));
        return;
    }

    router_.reset();
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::refreshSessionList()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::refreshSessionList, shared_from_this()));
        return;
    }

    if (router_)
        router_->refreshSessionList();
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::stopSession(int64_t session_id)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::stopSession, shared_from_this(), session_id));
        return;
    }

    if (router_)
        router_->stopSession(session_id);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::refreshUserList()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::refreshUserList, shared_from_this()));
        return;
    }

    if (router_)
        router_->refreshUserList();
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::addUser(const proto::User& user)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::addUser, shared_from_this(), user));
        return;
    }

    if (router_)
        router_->addUser(user);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::modifyUser(const proto::User& user)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::modifyUser, shared_from_this(), user));
        return;
    }

    if (router_)
        router_->modifyUser(user);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::deleteUser(int64_t entry_id)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::deleteUser, shared_from_this(), entry_id));
        return;
    }

    if (router_)
        router_->deleteUser(entry_id);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::Impl::disconnectPeerSession(int64_t relay_session_id, uint64_t peer_session_id)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(
            &Impl::disconnectPeerSession, shared_from_this(), relay_session_id, peer_session_id));
        return;
    }

    if (router_)
        router_->disconnectPeerSession(relay_session_id, peer_session_id);
}

//--------------------------------------------------------------------------------------------------
RouterProxy::RouterProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                         std::unique_ptr<Router> router)
    : impl_(std::make_shared<Impl>(std::move(io_task_runner), std::move(router)))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
RouterProxy::~RouterProxy()
{
    impl_->disconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::connectToRouter(const std::u16string& address, uint16_t port)
{
    impl_->connectToRouter(address, port);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::disconnectFromRouter()
{
    impl_->disconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::refreshSessionList()
{
    impl_->refreshSessionList();
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::stopSession(int64_t session_id)
{
    impl_->stopSession(session_id);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::refreshUserList()
{
    impl_->refreshUserList();
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::addUser(const proto::User& user)
{
    impl_->addUser(user);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::modifyUser(const proto::User& user)
{
    impl_->modifyUser(user);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::deleteUser(int64_t entry_id)
{
    impl_->deleteUser(entry_id);
}

//--------------------------------------------------------------------------------------------------
void RouterProxy::disconnectPeerSession(int64_t relay_session_id, uint64_t peer_session_id)
{
    impl_->disconnectPeerSession(relay_session_id, peer_session_id);
}

} // namespace client
