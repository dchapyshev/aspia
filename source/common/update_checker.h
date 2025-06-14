//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_UPDATE_CHECKER_H
#define COMMON_UPDATE_CHECKER_H

#include <QByteArray>
#include <QThread>

namespace common {

class UpdateChecker final : public QThread
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker();

    void setUpdateServer(const QString& update_server);
    void setPackageName(const QString& package_name);

signals:
    void sig_checkedFinished(const QByteArray& result);

protected:
    void run() final;

private:
    QString update_server_;
    QString package_name_;
    std::atomic_bool interrupted_ { false };

    Q_DISABLE_COPY(UpdateChecker)
};

} // namespace common

#endif // COMMON_UPDATE_CHECKER_H
