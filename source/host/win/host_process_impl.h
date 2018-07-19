//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_HOST__WIN__HOST_PROCESS_IMPL_H_
#define ASPIA_HOST__WIN__HOST_PROCESS_IMPL_H_

#include <QPointer>
#include <QString>
#include <QWinEventNotifier>

#include "base/win/scoped_object.h"
#include "host/win/host_process.h"

namespace aspia {

class HostProcessImpl
{
public:
    explicit HostProcessImpl(HostProcess* process);
    ~HostProcessImpl();

    void startProcess();
    void killProcess();

    bool startProcessWithToken(HANDLE token);

    HostProcess* process_;
    HostProcess::ProcessState state_ = HostProcess::NotRunning;
    HostProcess::Account account_ = HostProcess::User;
    quint32 session_id_ = -1;
    QString program_;
    QStringList arguments_;

    ScopedHandle thread_handle_;
    ScopedHandle process_handle_;

    QPointer<QWinEventNotifier> finish_notifier_;

    Q_DISABLE_COPY(HostProcessImpl)
};

} // namespace aspia

#endif // ASPIA_HOST__WIN__HOST_PROCESS_IMPL_H_
