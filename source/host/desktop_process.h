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

#ifndef HOST_DESKTOP_PROCESS_H
#define HOST_DESKTOP_PROCESS_H

#include <QObject>
#include <QString>

#include "base/session_id.h"

#if defined(Q_OS_WINDOWS)
#include <QWinEventNotifier>
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

class DesktopProcess final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopProcess(QObject* parent = nullptr);
    ~DesktopProcess() final;

    static QString filePath();

    enum class State
    {
        STOPPED,
        STARTING,
        ERROR_OCURRED,
        STARTED
    };
    Q_ENUM(State)

    State state() const;

public slots:
    void start(base::SessionId session_id, const QString& channel_name);
    void kill();

signals:
    void sig_stateChanged(host::DesktopProcess::State state);

private:
    void setState(State state);

#if defined(Q_OS_WINDOWS)
    base::ScopedHandle process_;
    base::ScopedHandle thread_;

    QWinEventNotifier* finish_notifier_ = nullptr;
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
    pid_t pid_;
#endif // defined(Q_OS_LINUX)

    State state_ = State::STOPPED;

    Q_DISABLE_COPY_MOVE(DesktopProcess)
};

} // namespace host

#endif // HOST_DESKTOP_PROCESS_H
