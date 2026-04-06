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

#ifndef CLIENT_UI_COMPUTERS_TAB_CONTENT_TREE_ITEM_H
#define CLIENT_UI_COMPUTERS_TAB_CONTENT_TREE_ITEM_H

#include <QTreeWidgetItem>

namespace client {

struct ComputerData;

class ContentTreeItem : public QTreeWidgetItem
{
public:
    enum class Type { LOCAL_COMPUTER, REMOTE_COMPUTER, SEARCH_COMPUTER };

    Type itemType() const;
    qint64 computerId() const;

protected:
    ContentTreeItem(Type type, const ComputerData& computer, QTreeWidget* parent);

private:
    Type type_;
    qint64 computer_id_;
};

class LocalComputerItem : public ContentTreeItem
{
public:
    LocalComputerItem(const ComputerData& computer, QTreeWidget* parent);
};

class RemoteComputerItem : public ContentTreeItem
{
public:
    RemoteComputerItem(const ComputerData& computer, QTreeWidget* parent);
};

class SearchComputerItem : public ContentTreeItem
{
public:
    SearchComputerItem(const ComputerData& computer, QTreeWidget* parent);
};

} // namespace client

#endif // CLIENT_UI_COMPUTERS_TAB_CONTENT_TREE_ITEM_H
