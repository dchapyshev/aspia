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

#ifndef BASE_SERVICE_H
#define BASE_SERVICE_H

#include "base/application.h"
#include "base/macros_magic.h"
#include "base/session_id.h"
#include "base/task_runner.h"

#if defined(OS_WIN)
#include "base/win/session_status.h"
#endif // defined(OS_WIN)

#include <QString>

namespace base {

namespace {
class ServiceThread;
} // namespace

class Service
{
public:
    explicit Service(const QString& name);
    virtual ~Service();

    int exec(Application& application);

    const QString& name() const { return name_; }
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
    void stopHandlerImpl();
    static void signalHandler(int sig);
#endif // defined(OS_POSIX)

    QString name_;
    std::shared_ptr<TaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace base

#endif // BASE_SERVICE_H
