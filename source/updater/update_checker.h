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

#ifndef UPDATER__UPDATE_CHECKER_H
#define UPDATER__UPDATE_CHECKER_H

#include <QPointer>

#include "base/macros_magic.h"
#include "updater/update_info.h"

class QThread;

namespace updater {

class CheckerImpl;

Q_DECLARE_METATYPE(UpdateInfo);

class Checker : public QObject
{
    Q_OBJECT

public:
    explicit Checker(QObject* parent = nullptr);
    ~Checker();

    void setUpdateServer(const QString& update_server);
    void setPackageName(const QString& package_name);

    void start();

signals:
    void finished(const UpdateInfo& update_info);

private:
    QPointer<QThread> thread_;
    QPointer<CheckerImpl> impl_;

    DISALLOW_COPY_AND_ASSIGN(Checker);
};

} // namespace updater

#endif // UPDATER__UPDATE_CHECKER_H
