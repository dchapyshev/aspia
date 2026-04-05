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

#include "client/ui/local_tab.h"

#include "base/logging.h"

#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidgetItem>

namespace client {

namespace {

const int kColumnName = 0;
const int kColumnAddress = 1;
const int kColumnComment = 2;

const int kGroupIdRole = Qt::UserRole;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalTab::LocalTab(QWidget* parent)
    : ClientTab(Type::LOCAL, parent)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    // Open or create database.
    if (BookDatabase::isEncrypted())
    {
        // TODO: Ask for password.
        database_ = BookDatabase::open();
    }
    else
    {
        database_ = BookDatabase::open();
        if (!database_.isValid())
            database_ = BookDatabase::create(BookDatabase::EncryptionType::NONE);
    }

    if (!database_.isValid())
    {
        LOG(ERROR) << "Failed to open or create book database";
        return;
    }

    // Create toolbar actions.
    action_add_group_ = new QAction(QIcon(":/img/add-folder.svg"), tr("Add Group"), this);
    action_add_computer_ = new QAction(QIcon(":/img/add-computer.svg"), tr("Add Computer"), this);
    action_delete_ = new QAction(QIcon(":/img/recycle-bin.svg"), tr("Delete"), this);

    // Load group tree.
    loadGroups(0, nullptr);

    // Show computers in root group.
    updateComputerList(0);

    // Connect signals.
    connect(ui.tree_group, &QTreeWidget::itemClicked, this, &LocalTab::onGroupItemClicked);
}

//--------------------------------------------------------------------------------------------------
LocalTab::~LocalTab()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void LocalTab::onActivated(QToolBar* toolbar, QStatusBar* statusbar)
{
    // Find the first global action (action_settings) to insert before it.
    QAction* before = nullptr;
    QList<QAction*> actions = toolbar->actions();
    if (!actions.isEmpty())
        before = actions.first();

    addToolbarAction(toolbar, action_add_group_, before);
    addToolbarAction(toolbar, action_add_computer_, before);
    addToolbarAction(toolbar, action_delete_, before);
    separator_before_global_ = toolbar->insertSeparator(before);

    // Update statusbar.
    statusbar->showMessage(tr("Computers: %1").arg(ui.tree_computer->topLevelItemCount()));
}

//--------------------------------------------------------------------------------------------------
void LocalTab::onDeactivated(QToolBar* toolbar, QStatusBar* statusbar)
{
    removeAllToolbarActions(toolbar);

    if (separator_before_global_)
    {
        toolbar->removeAction(separator_before_global_);
        separator_before_global_ = nullptr;
    }

    statusbar->clearMessage();
}

//--------------------------------------------------------------------------------------------------
bool LocalTab::hasSearchField() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
void LocalTab::onSearchTextChanged(const QString& text)
{
    if (text.isEmpty())
    {
        updateComputerList(current_group_id_);
    }
    else
    {
        QList<ComputerData> results = database_.searchComputers(text);
        showComputerList(results);
    }
}

//--------------------------------------------------------------------------------------------------
void LocalTab::loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item)
{
    QList<ComputerGroupData> groups = database_.groupList(parent_id);

    for (const ComputerGroupData& group : std::as_const(groups))
    {
        QTreeWidgetItem* item = nullptr;

        if (parent_item)
            item = new QTreeWidgetItem(parent_item);
        else
            item = new QTreeWidgetItem(ui.tree_group);

        item->setText(0, group.name);
        item->setIcon(0, QIcon(":/img/folder.svg"));
        item->setData(0, kGroupIdRole, group.id);
        item->setExpanded(group.expanded);

        // Load child groups recursively.
        loadGroups(group.id, item);
    }
}

//--------------------------------------------------------------------------------------------------
void LocalTab::updateComputerList(qint64 group_id)
{
    current_group_id_ = group_id;
    QList<ComputerData> computers = database_.computerList(group_id);
    showComputerList(computers);
}

//--------------------------------------------------------------------------------------------------
void LocalTab::showComputerList(const QList<ComputerData>& computers)
{
    ui.tree_computer->clear();

    for (const ComputerData& computer : std::as_const(computers))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(ui.tree_computer);
        item->setText(kColumnName, computer.name);
        item->setText(kColumnAddress, computer.address);
        item->setText(kColumnComment, computer.comment);
        item->setIcon(kColumnName, QIcon(":/img/computer.svg"));
        item->setData(kColumnName, Qt::UserRole, computer.id);
    }
}

//--------------------------------------------------------------------------------------------------
void LocalTab::onGroupItemClicked(QTreeWidgetItem* item, int /* column */)
{
    if (!item)
        return;

    qint64 group_id = item->data(0, kGroupIdRole).toLongLong();
    updateComputerList(group_id);
}

} // namespace client
