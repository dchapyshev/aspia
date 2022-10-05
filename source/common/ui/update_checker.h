//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_UI_UPDATE_CHECKER_H
#define COMMON_UI_UPDATE_CHECKER_H

#include "base/macros_magic.h"
#include "common/ui/update_info.h"

#include <QObject>
#include <QPointer>

class QThread;

namespace common {

class UpdateCheckerImpl;

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    void setUpdateServer(const QString& update_server);
    void setPackageName(const QString& package_name);

    void start();

signals:
    void finished(const QByteArray& result);

private:
    QPointer<QThread> thread_;
    QPointer<UpdateCheckerImpl> impl_;

    DISALLOW_COPY_AND_ASSIGN(UpdateChecker);
};

} // namespace common

#endif // COMMON_UI_UPDATE_CHECKER_H
