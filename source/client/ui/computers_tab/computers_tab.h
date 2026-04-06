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

#ifndef CLIENT_UI_COMPUTERS_TAB_COMPUTERS_TAB_H
#define CLIENT_UI_COMPUTERS_TAB_COMPUTERS_TAB_H

#include "client/ui/client_tab.h"
#include "ui_computers_tab.h"

class QTreeWidgetItem;

namespace client {

class ContentWidget;
class LocalGroupItem;
class LocalGroupWidget;
class RouterItem;
class RouterWidget;
class RouterGroupWidget;
class SearchWidget;

class ComputersTab : public ClientTab
{
    Q_OBJECT

public:
    explicit ComputersTab(QWidget* parent = nullptr);
    ~ComputersTab() override;

    void onActivated(QToolBar* toolbar, QStatusBar* statusbar) override;
    void onDeactivated(QToolBar* toolbar, QStatusBar* statusbar) override;
    bool hasSearchField() const override;
    void onSearchTextChanged(const QString& text) override;

private slots:
    void onAddComputerAction();
    void onEditComputerAction();
    void onDeleteComputerAction();
    void onAddGroupAction();
    void onEditGroupAction();
    void onDeleteGroupAction();
    void onCurrentGroupChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onCurrentComputerChanged(qint64 computer_id);

private:
    void loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item);
    void switchContent(ContentWidget* new_widget);
    void updateActionsState();

    Ui::ComputersTab ui;
    qint64 current_group_id_ = 0;

    LocalGroupItem* local_root_ = nullptr;
    RouterItem* remote_root_ = nullptr;

    QAction* action_add_group_ = nullptr;
    QAction* action_add_computer_ = nullptr;
    QAction* action_delete_group_ = nullptr;
    QAction* action_delete_computer_ = nullptr;
    QAction* action_edit_group_ = nullptr;
    QAction* action_edit_computer_ = nullptr;

    ContentWidget* current_content_ = nullptr;
    ContentWidget* previous_content_ = nullptr;

    LocalGroupWidget* local_group_widget_ = nullptr;
    RouterWidget* router_widget_ = nullptr;
    RouterGroupWidget* router_group_widget_ = nullptr;
    SearchWidget* search_widget_ = nullptr;

    Q_DISABLE_COPY_MOVE(ComputersTab)
};

} // namespace client

#endif // CLIENT_UI_COMPUTERS_TAB_COMPUTERS_TAB_H
