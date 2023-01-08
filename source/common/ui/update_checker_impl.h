//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_UI_UPDATE_CHECKER_IMPL_H
#define COMMON_UI_UPDATE_CHECKER_IMPL_H

#include "base/macros_magic.h"
#include "common/ui/update_info.h"

#include <QObject>

namespace common {

class UpdateCheckerImpl : public QObject
{
    Q_OBJECT

public:
    explicit UpdateCheckerImpl(QObject* parent = nullptr);
    ~UpdateCheckerImpl() override;

    void setUpdateServer(const QString& update_server);
    void setPackageName(const QString& package_name);

signals:
    void finished(const QByteArray& result);

public slots:
    void start();

private:
    QString update_server_;
    QString package_name_;

    DISALLOW_COPY_AND_ASSIGN(UpdateCheckerImpl);
};

} // namespace common

#endif // COMMON_UI_UPDATE_CHECKER_IMPL_H
