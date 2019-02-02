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

#ifndef UPDATER__UPDATE_CHECKER_IMPL_H
#define UPDATER__UPDATE_CHECKER_IMPL_H

#include <QObject>

#include "base/macros_magic.h"
#include "updater/update_info.h"

class QNetworkAccessManager;

namespace updater {

class CheckerImpl : public QObject
{
    Q_OBJECT

public:
    explicit CheckerImpl(QObject* parent = nullptr);
    ~CheckerImpl();

    void setUpdateServer(const QString& update_server);
    void setPackageName(const QString& package_name);

signals:
    void finished(const UpdateInfo& update_info);

public slots:
    void start();

private:
    QNetworkAccessManager* network_manager_;

    QString update_server_;
    QString package_name_;

    DISALLOW_COPY_AND_ASSIGN(CheckerImpl);
};

} // namespace updater

#endif // UPDATER__UPDATE_CHECKER_IMPL_H
