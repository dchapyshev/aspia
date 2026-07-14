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

#ifndef BASE_CORE_SERVICE_H
#define BASE_CORE_SERVICE_H

#include <QString>

#include "base/core_application.h"

namespace {
class ServiceThread;
} // namespace

class CoreService : public QObject
{
    Q_OBJECT

public:
    CoreService(const QString& name, QObject* parent);
    virtual ~CoreService();

    int exec(CoreApplication& application);

    const QString& name() const { return name_; }

signals:
    void sig_sessionEvent(quint32 event, quint32 session_id);
    void sig_powerEvent(quint32 event);

protected:
    friend class ServiceThread;

    virtual void onStart() = 0;
    virtual void onStop() = 0;

private:
#if defined(Q_OS_UNIX)
    void onSignalActivated();
    static void signalHandler(int sig);

    int signal_fd_[2] = { -1, -1 };
#endif // defined(Q_OS_UNIX)

    QString name_;

    Q_DISABLE_COPY_MOVE(CoreService)
};

#endif // BASE_CORE_SERVICE_H
