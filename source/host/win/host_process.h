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

#ifndef ASPIA_HOST__WIN__HOST_PROCESS_H_
#define ASPIA_HOST__WIN__HOST_PROCESS_H_

#include <QObject>
#include <QScopedPointer>

#include "base/macros_magic.h"

namespace aspia {

class HostProcessImpl;

class HostProcess : public QObject
{
    Q_OBJECT

public:
    enum ProcessState
    {
        NotRunning,
        Starting,
        Running
    };
    Q_ENUM(ProcessState)

    enum Account
    {
        System,
        User
    };
    Q_ENUM(Account)

    enum ErrorCode
    {
        NoError,
        NoLoggedOnUser,
        OtherError
    };
    Q_ENUM(ErrorCode)

    HostProcess(QObject* parent = nullptr);
    virtual ~HostProcess();

    void start(uint32_t session_id,
               Account account,
               const QString& program,
               const QStringList& arguments);

    uint32_t sessionId() const;
    void setSessionId(uint32_t session_id);

    Account account() const;
    void setAccount(Account account);

    QString program() const;
    void setProgram(const QString& program);

    QStringList arguments() const;
    void setArguments(const QStringList& arguments);

    ProcessState state() const;

public slots:
    void start();
    void kill();
    void terminate();

signals:
    void started();
    void finished();
    void errorOccurred(HostProcess::ErrorCode error_code);

private:
    friend class HostProcessImpl;

    QScopedPointer<HostProcessImpl> impl_;

    DISALLOW_COPY_AND_ASSIGN(HostProcess);
};

} // namespace aspia

#endif // ASPIA_HOST__WIN__HOST_PROCESS_H_
