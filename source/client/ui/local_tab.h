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

#ifndef CLIENT_UI_LOCAL_TAB_H
#define CLIENT_UI_LOCAL_TAB_H

#include "client/book_database.h"
#include "client/ui/client_tab.h"
#include "ui_local_tab.h"

class QTreeWidgetItem;

namespace client {

class LocalTab : public ClientTab
{
    Q_OBJECT

public:
    explicit LocalTab(QWidget* parent = nullptr);
    ~LocalTab() override;

    void onActivated(QToolBar* toolbar, QStatusBar* statusbar) override;
    void onDeactivated(QToolBar* toolbar, QStatusBar* statusbar) override;
    bool hasSearchField() const override;
    void onSearchTextChanged(const QString& text) override;

private:
    void loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item);
    void updateComputerList(qint64 group_id);
    void showComputerList(const QList<ComputerData>& computers);
    void onGroupItemClicked(QTreeWidgetItem* item, int column);

    Ui::LocalTab ui;
    BookDatabase database_;
    qint64 current_group_id_ = 0;

    QAction* action_add_group_ = nullptr;
    QAction* action_add_computer_ = nullptr;
    QAction* action_delete_ = nullptr;
    QAction* separator_before_global_ = nullptr;

    Q_DISABLE_COPY_MOVE(LocalTab)
};

} // namespace client

#endif // CLIENT_UI_LOCAL_TAB_H
