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

#include "host/user_session_process_proxy.h"

#include "base/message_loop/message_loop_task_runner.h"
#include "qt_base/application.h"

#include <variant>

#include <QEvent>

namespace host {

namespace {

class UserSessionEvent : public QEvent
{
public:
    static const int kType = QEvent::User + 1;

    using EventData = std::variant<UserSessionProcess::State,
                                   UserSessionProcess::ClientList,
                                   proto::Credentials>;

    UserSessionEvent(const EventData& event_data)
        : QEvent(QEvent::Type(kType)),
          event_data(event_data)
    {
        // Nothing
    }

    const EventData event_data;

private:
    DISALLOW_COPY_AND_ASSIGN(UserSessionEvent);
};

} // namespace

UserSessionProcessProxy::UserSessionProcessProxy(std::unique_ptr<UserSessionProcess> process)
    : process_(std::move(process)),
      runner_(qt_base::Application::taskRunner())
{
    // Nothing
}

UserSessionProcessProxy::~UserSessionProcessProxy() = default;

void UserSessionProcessProxy::updateCredentials(proto::CredentialsRequest::Type request_type)
{
    if (!runner_->belongsToCurrentThread())
    {
        runner_->postTask(
            std::bind(&UserSessionProcessProxy::updateCredentials, this, request_type));
        return;
    }

    process_->updateCredentials(request_type);
}

void UserSessionProcessProxy::killClient(const std::string& uuid)
{
    if (!runner_->belongsToCurrentThread())
    {
        runner_->postTask(std::bind(&UserSessionProcessProxy::killClient, this, uuid));
        return;
    }

    process_->killClient(uuid);
}

void UserSessionProcessProxy::customEvent(QEvent* event)
{
    if (event->type() != UserSessionEvent::kType)
        return;

    const UserSessionEvent::EventData& event_data =
        reinterpret_cast<UserSessionEvent*>(event)->event_data;

    if (std::holds_alternative<UserSessionProcess::State>(event_data))
    {
        emit stateChanged(std::get<UserSessionProcess::State>(event_data));
    }
    else if (std::holds_alternative<UserSessionProcess::ClientList>(event_data))
    {
        emit clientListChanged(std::get<UserSessionProcess::ClientList>(event_data));
    }
    else if (std::holds_alternative<proto::Credentials>(event_data))
    {
        emit credentialsChanged(std::get<proto::Credentials>(event_data));
    }
}

void UserSessionProcessProxy::onStateChanged()
{
    QApplication::postEvent(this, new UserSessionEvent(process_->state()));
}

void UserSessionProcessProxy::onClientListChanged()
{
    QApplication::postEvent(this, new UserSessionEvent(process_->clients()));
}

void UserSessionProcessProxy::onCredentialsChanged()
{
    QApplication::postEvent(this, new UserSessionEvent(process_->credentials()));
}

} // namespace host
