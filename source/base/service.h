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

#include <QString>

#include "base/application.h"

namespace base {

namespace {
class ServiceThread;
} // namespace

class Service : public QObject
{
    Q_OBJECT

public:
    Service(const QString& name, QObject* parent);
    virtual ~Service();

    int exec(Application& application);

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
    void stopHandlerImpl();
    static void signalHandler(int sig);
#endif // defined(Q_OS_UNIX)

    QString name_;

    Q_DISABLE_COPY(Service)
};

} // namespace base

#endif // BASE_SERVICE_H
