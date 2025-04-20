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

#ifndef BASE_THREADING_ASIO_THREAD_H
#define BASE_THREADING_ASIO_THREAD_H

#include "base/macros_magic.h"
#include "base/task_runner.h"

#include <QThread>

namespace base {

class AsioThread final : public QThread
{
public:
    enum class EventDispatcher { ASIO, QT };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called just prior to starting the message loop.
        virtual void onBeforeThreadRunning()
        {
            // Nothing
        }

        // Called just after the message loop ends.
        virtual void onAfterThreadRunning()
        {
            // Nothing
        }
    };

    AsioThread(EventDispatcher event_dispatcher, Delegate* delegate, QObject* parent = nullptr);

    void stop();

    static std::shared_ptr<base::TaskRunner> currentTaskRunner();
    std::shared_ptr<base::TaskRunner> taskRunner();

protected:
    // QThread implementation.
    void run() final;

private:
    std::shared_ptr<TaskRunner> task_runner_;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(AsioThread);
};

} // namespace base

#endif // BASE_THREADING_ASIO_THREAD_H
