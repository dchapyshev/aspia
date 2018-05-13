//
// PROJECT:         Aspia
// FILE:            host/win/host_process_impl.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__WIN__HOST_PROCESS_IMPL_H
#define _ASPIA_HOST__WIN__HOST_PROCESS_IMPL_H

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

#endif // _ASPIA_HOST__WIN__HOST_PROCESS_IMPL_H
