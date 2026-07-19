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

#ifndef BASE_CORE_APPLICATION_H
#define BASE_CORE_APPLICATION_H

#include <QCoreApplication>

#include <memory>

#include "base/scoped_qpointer.h"
#include "base/threading/worker_manager.h"

class QSocketNotifier;
class MessageWindow;
class Thread;
class EventMonitor;

struct sd_login_monitor;

class CoreApplication final : public QCoreApplication
{
    Q_OBJECT

public:
    CoreApplication(int& argc, char* argv[]);
    ~CoreApplication() final;

    int exec();

    static CoreApplication* instance();

    template <typename T>
    static T* findWorker()
    {
        CoreApplication* application = instance();
        return application ? application->worker_manager_->find<T>() : nullptr;
    }

    qint64 addWorker(std::unique_ptr<Worker> worker);

signals:
    void sig_queryEndSession();
    void sig_sessionEvent(quint32 event, quint32 session_id);
    void sig_powerEvent(quint32 event);

private:
    std::unique_ptr<WorkerManager> worker_manager_;

#if defined(Q_OS_WINDOWS)
    std::unique_ptr<Thread> ui_thread_;
    std::unique_ptr<MessageWindow> message_window_;
    bool is_service_ = false;
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    sd_login_monitor* login_monitor_ = nullptr;
    ScopedQPointer<QSocketNotifier> session_notifier_;
    int last_active_session_ = -1;
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#if defined(Q_OS_MACOS)
    std::unique_ptr<EventMonitor> event_monitor_;
#endif // defined(Q_OS_MACOS)

    Q_DISABLE_COPY_MOVE(CoreApplication)
};

#endif // BASE_CORE_APPLICATION_H
