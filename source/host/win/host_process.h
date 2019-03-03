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

#ifndef HOST__WIN__HOST_PROCESS_H
#define HOST__WIN__HOST_PROCESS_H

#include "base/win/process.h"
#include "base/win/session_id.h"

#include <QObject>
#include <QPointer>

namespace host {

class HostProcessImpl;

class HostProcess : public QObject
{
    Q_OBJECT

public:
    enum ProcessState { NotRunning, Starting, Running };
    enum Account { System, User };
    enum ErrorCode { NoError, NoLoggedOnUser, OtherError };

    HostProcess(QObject* parent = nullptr);
    virtual ~HostProcess();

    ErrorCode start();

    ErrorCode start(base::win::SessionId session_id,
                    Account account,
                    const QString& program,
                    const QStringList& arguments);

    base::win::SessionId sessionId() const;
    void setSessionId(base::win::SessionId session_id);

    Account account() const;
    void setAccount(Account account);

    QString program() const;
    void setProgram(const QString& program);

    QStringList arguments() const;
    void setArguments(const QStringList& arguments);

    ProcessState state() const;

public slots:
    void kill();
    void terminate();

signals:
    void started();
    void finished();

private:
    QPointer<base::win::Process> process_;

    HostProcess::ProcessState state_ = HostProcess::NotRunning;
    HostProcess::Account account_ = HostProcess::User;
    base::win::SessionId session_id_ = base::win::kInvalidSessionId;
    QString program_;
    QStringList arguments_;

    DISALLOW_COPY_AND_ASSIGN(HostProcess);
};

} // namespace host

#endif // HOST__WIN__HOST_PROCESS_H
