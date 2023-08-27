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

#ifndef BASE_SERVICE_H
#define BASE_SERVICE_H

#include "base/message_loop/message_loop.h"
#include "base/session_id.h"

#if defined(OS_WIN)
#include "base/win/session_status.h"
#endif // defined(OS_WIN)

#include <string_view>
#include <string>

namespace base {

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

#if defined(OS_WIN)
    virtual void onSessionEvent(win::SessionStatus event, SessionId session_id) = 0;
    virtual void onPowerEvent(uint32_t event) = 0;
#endif // defined(OS_WIN)

private:
#if defined(OS_POSIX)
    void signalHandlerImpl(int sig);
    static void signalHandler(int sig);
#endif // defined(OS_POSIX)

    MessageLoop::Type type_;
    std::u16string name_;
    std::unique_ptr<MessageLoop> message_loop_;
    std::shared_ptr<TaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace base

#endif // BASE_SERVICE_H
