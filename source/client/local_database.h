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

#ifndef CLIENT_LOCAL_DATABASE_H
#define CLIENT_LOCAL_DATABASE_H

#include <QList>
#include <QString>

#include <optional>

#include "client/local_data.h"

namespace client {

class LocalDatabase
{
public:
    ~LocalDatabase() = default;

    static LocalDatabase& instance();
    static QString filePath();
    bool isValid() const;

    // Computers.
    QList<ComputerData> computerList(qint64 group_id) const;
    bool addComputer(ComputerData& computer);
    bool modifyComputer(const ComputerData& computer);
    bool removeComputer(qint64 computer_id);
    std::optional<ComputerData> findComputer(qint64 computer_id) const;

    // Search.
    QList<ComputerData> searchComputers(const QString& query) const;

    // Groups.
    QList<GroupData> groupList(qint64 parent_id) const;
    QList<GroupData> allGroups() const;
    bool addGroup(GroupData& group);
    bool modifyGroup(const GroupData& group);
    bool removeGroup(qint64 group_id);
    std::optional<GroupData> findGroup(qint64 group_id) const;

private:
    LocalDatabase() = default;

    bool openDatabase();

    bool valid_ = false;
};

} // namespace client

#endif // CLIENT_LOCAL_DATABASE_H
