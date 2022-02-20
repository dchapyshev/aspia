//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_SERVICE_H
#define BASE_WIN_SERVICE_H

#include "base/message_loop/message_loop.h"
#include "base/session_id.h"
#include "base/win/session_status.h"

namespace base::win {

namespace {
class ServiceThread;
} // namespace

class Service
{
public:
    Service(std::u16string_view name, MessageLoop::Type type);
    virtual ~Service();

    void exec();

    const std::u16string& name() const { return name_; }
    MessageLoop* messageLoop() const { return message_loop_.get(); }
    std::shared_ptr<TaskRunner> taskRunner() const { return task_runner_; }

protected:
    friend class ServiceThread;

    virtual void onStart() = 0;
    virtual void onStop() = 0;
    virtual void onSessionEvent(SessionStatus event, SessionId session_id) = 0;
    virtual void onPowerEvent(uint32_t event) = 0;

private:
    MessageLoop::Type type_;
    std::u16string name_;
    std::unique_ptr<MessageLoop> message_loop_;
    std::shared_ptr<TaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace base::win

#endif // BASE_WIN_SERVICE_H
