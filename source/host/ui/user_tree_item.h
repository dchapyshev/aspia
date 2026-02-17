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

#ifndef HOST_UI_USER_TREE_ITEM_H
#define HOST_UI_USER_TREE_ITEM_H

#include <QCollator>
#include <QTreeWidget>

#include "base/peer/user.h"

namespace host {

class User;

class UserTreeItem final : public QTreeWidgetItem
{
public:
    explicit UserTreeItem(const base::User& user);
    ~UserTreeItem() final = default;

    const base::User& user() const { return user_; }
    void setUser(const base::User& user);

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 0)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(0), other.text(0)) <= 0;
        }
        else
        {
            return QTreeWidgetItem::operator<(other);
        }
    }

private:
    void updateData();

    base::User user_;
    Q_DISABLE_COPY(UserTreeItem)
};

} // namespace host

#endif // HOST_UI_USER_TREE_ITEM_H
