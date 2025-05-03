//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/user_session_agent_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace host {

class UserSessionAgentProxy::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<base::TaskRunner> io_task_runner,
         std::unique_ptr<UserSessionAgent> agent);
    ~Impl();

    void start();
    void stop();
    void updateCredentials(proto::internal::CredentialsRequest::Type request_type);
    void setOneTimeSessions(quint32 sessions);
    void killClient(quint32 id);
    void connectConfirmation(quint32 id, bool accept);
    void setVoiceChat(bool enable);
    void setMouseLock(bool enable);
    void setKeyboardLock(bool enable);
    void setPause(bool enable);
    void onTextChat(const proto::TextChat& text_chat);

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::unique_ptr<UserSessionAgent> agent_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
UserSessionAgentProxy::Impl::Impl(std::shared_ptr<base::TaskRunner> io_task_runner,
                                  std::unique_ptr<UserSessionAgent> agent)
    : io_task_runner_(std::move(io_task_runner)),
      agent_(std::move(agent))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(io_task_runner_ && agent_);
}

//--------------------------------------------------------------------------------------------------
UserSessionAgentProxy::Impl::~Impl()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!agent_);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::start()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::start, shared_from_this()));
        return;
    }

    if (agent_)
        agent_->start();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::stop()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::stop, shared_from_this()));
        return;
    }

    agent_.reset();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::updateCredentials(
    proto::internal::CredentialsRequest::Type request_type)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&Impl::updateCredentials, shared_from_this(), request_type));
        return;
    }

    if (agent_)
        agent_->updateCredentials(request_type);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::setOneTimeSessions(quint32 sessions)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&Impl::setOneTimeSessions, shared_from_this(), sessions));
        return;
    }

    if (agent_)
        agent_->setOneTimeSessions(sessions);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::killClient(quint32 id)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::killClient, shared_from_this(), id));
        return;
    }

    if (agent_)
        agent_->killClient(id);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::connectConfirmation(quint32 id, bool accept)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&Impl::connectConfirmation, shared_from_this(), id, accept));
        return;
    }

    if (agent_)
        agent_->connectConfirmation(id, accept);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::setVoiceChat(bool enable)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::setVoiceChat, shared_from_this(), enable));
        return;
    }

    if (agent_)
        agent_->setVoiceChat(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::setMouseLock(bool enable)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::setMouseLock, shared_from_this(), enable));
        return;
    }

    if (agent_)
        agent_->setMouseLock(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::setKeyboardLock(bool enable)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::setKeyboardLock, shared_from_this(), enable));
        return;
    }

    if (agent_)
        agent_->setKeyboardLock(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::setPause(bool enable)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::setPause, shared_from_this(), enable));
        return;
    }

    if (agent_)
        agent_->setPause(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::Impl::onTextChat(const proto::TextChat& text_chat)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::onTextChat, shared_from_this(), text_chat));
        return;
    }

    if (agent_)
        agent_->onTextChat(text_chat);
}

//--------------------------------------------------------------------------------------------------
UserSessionAgentProxy::UserSessionAgentProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                                             std::unique_ptr<UserSessionAgent> agent)
    : impl_(std::make_shared<Impl>(std::move(io_task_runner), std::move(agent)))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
UserSessionAgentProxy::~UserSessionAgentProxy()
{
    LOG(LS_INFO) << "Dtor";
    impl_->stop();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::start()
{
    impl_->start();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::stop()
{
    impl_->stop();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::updateCredentials(proto::internal::CredentialsRequest::Type request_type)
{
    impl_->updateCredentials(request_type);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::setOneTimeSessions(quint32 sessions)
{
    impl_->setOneTimeSessions(sessions);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::killClient(quint32 id)
{
    impl_->killClient(id);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::connectConfirmation(quint32 id, bool accept)
{
    impl_->connectConfirmation(id, accept);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::setVoiceChat(bool enable)
{
    impl_->setVoiceChat(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::setMouseLock(bool enable)
{
    impl_->setMouseLock(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::setKeyboardLock(bool enable)
{
    impl_->setKeyboardLock(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::setPause(bool enable)
{
    impl_->setPause(enable);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgentProxy::onTextChat(const proto::TextChat& text_chat)
{
    impl_->onTextChat(text_chat);
}

} // namespace host
