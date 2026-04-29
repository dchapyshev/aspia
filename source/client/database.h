//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_DATABASE_H
#define CLIENT_DATABASE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <optional>

#include "client/config.h"

namespace client {

class Database
{
public:
    ~Database() = default;

    static Database& instance();
    static QString filePath();
    bool isValid() const;

    // Computers.
    QList<ComputerConfig> computerList(qint64 group_id) const;
    bool addComputer(ComputerConfig& computer);
    bool modifyComputer(ComputerConfig& computer);
    bool removeComputer(qint64 computer_id);
    bool setConnectTime(qint64 computer_id, qint64 connect_time);
    std::optional<ComputerConfig> findComputer(qint64 computer_id) const;

    // Search.
    QList<ComputerConfig> searchComputers(const QString& query) const;

    // Groups.
    QList<GroupConfig> groupList(qint64 parent_id) const;
    QList<GroupConfig> allGroups() const;
    bool addGroup(GroupConfig& group);
    bool modifyGroup(const GroupConfig& group);
    bool moveGroup(qint64 group_id, qint64 new_parent_id);
    bool removeGroup(qint64 group_id);
    std::optional<GroupConfig> findGroup(qint64 group_id) const;

    // Routers.
    QList<RouterConfig> routerList() const;
    bool addRouter(RouterConfig& router);
    bool modifyRouter(const RouterConfig& router);
    bool removeRouter(qint64 router_id);
    std::optional<RouterConfig> findRouter(qint64 router_id) const;

    // Settings.
    bool setProperty(const QString& name, const QVariant& value);
    QVariant property(const QString& name, const QVariant& default_value = {}) const;
    bool removeProperty(const QString& name);
    bool hasProperty(const QString& name) const;
    QStringList propertyNames() const;

private:
    Database() = default;

    bool openDatabase();

    bool valid_ = false;
};

} // namespace client

#endif // CLIENT_DATABASE_H
