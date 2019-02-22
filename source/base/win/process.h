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

#include <QWinEventNotifier>

#include "base/win/scoped_object.h"

namespace base::win {

class Process : public QObject
{
    Q_OBJECT

public:
    Process(uint32_t process_id, QObject* parent = nullptr);
    ~Process();

    static QString createCommandLine(const QString& program, const QStringList& arguments);
    static QString normalizedProgram(const QString& program);
    static QString createParamaters(const QStringList& arguments);

    bool isValid() const;

    QString filePath() const;
    QString fileName() const;
    uint32_t processId() const;
    uint32_t sessionId() const;

    void kill();
    void terminate();

signals:
    void finished();

protected:
    Process(HANDLE process, HANDLE thread, QObject* parent = nullptr);

private:
    QWinEventNotifier* notifier_ = nullptr;

    ScopedHandle process_;
    ScopedHandle thread_;

    DISALLOW_COPY_AND_ASSIGN(Process);
};

} // namespace base::win

#endif // BASE__WIN__PROCESS_H
