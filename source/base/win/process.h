//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__WIN__PROCESS_H
#define BASE__WIN__PROCESS_H

#include "base/process_handle.h"
#include "base/win/scoped_object.h"
#include "base/win/session_id.h"

#include <QWinEventNotifier>

namespace base::win {

class Process : public QObject
{
    Q_OBJECT

public:
    Process(ProcessId process_id, QObject* parent = nullptr);
    Process(HANDLE process, HANDLE thread, QObject* parent = nullptr);
    ~Process();

    static QString createCommandLine(const QString& program, const QStringList& arguments);
    static QString normalizedProgram(const QString& program);
    static QString createParamaters(const QStringList& arguments);

    bool isValid() const;

    QString filePath() const;
    QString fileName() const;
    ProcessId processId() const;
    SessionId sessionId() const;

    int exitCode() const;

    void kill();
    void terminate();

    HANDLE native() const { return process_.get(); }

signals:
    void finished(int exit_code);

private:
    void initNotifier();

    enum class State { INVALID, STARTED, FINISHED };

    QWinEventNotifier* notifier_ = nullptr;

    ScopedHandle process_;
    ScopedHandle thread_;

    State state_ = State::INVALID;

    DISALLOW_COPY_AND_ASSIGN(Process);
};

} // namespace base::win

#endif // BASE__WIN__PROCESS_H
