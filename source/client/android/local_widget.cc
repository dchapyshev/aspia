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

#include "client/android/local_widget.h"

#include <QHeaderView>
#include <QStackedWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "base/crypto/data_cryptor.h"
#include "base/gui_application.h"
#include "client/android/local_host_editor.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/icon_button.h"
#include "common/android/menu.h"
#include "common/android/tree_widget.h"

namespace {

constexpr int kPageTree = 0;
constexpr int kPageHosts = 1;
constexpr int kPageEditor = 2;

// Item data roles. A host row (an ungrouped host shown at the root) carries kHostIdRole; a group
// row carries kGroupIdRole.
constexpr int kGroupIdRole = Qt::UserRole;
constexpr int kHostIdRole = Qt::UserRole + 1;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalWidget::LocalWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      tree_(new TreeWidget(this)),
      host_tree_(new TreeWidget(this)),
      editor_(new LocalHostEditor(this)),
      search_button_(new IconButton(":/img/material/search.svg", this)),
      overflow_button_(new IconButton(":/img/material/more_vert.svg", this))
{
    // The action buttons live in the app bar; AppBar::setActions() reparents and shows the ones it
    // receives. Hidden by default so they do not linger in this widget.
    search_button_->hide();
    overflow_button_->hide();

    // Two columns: the entry name and, for ungrouped hosts shown at the root, its address.
    tree_->setColumnCount(2);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    // Tree page: the group accordion.
    QWidget* tree_page = new QWidget(stack_);
    QVBoxLayout* tree_layout = new QVBoxLayout(tree_page);
    tree_layout->setContentsMargins(0, 0, 0, 0);
    tree_layout->setSpacing(0);
    tree_layout->addWidget(tree_);

    // Hosts page: the two-column host list. Its title and back button live in the app bar.
    QWidget* host_page = new QWidget(stack_);
    QVBoxLayout* host_layout = new QVBoxLayout(host_page);
    host_layout->setContentsMargins(0, 0, 0, 0);
    host_layout->setSpacing(0);

    host_tree_->setRootIsDecorated(false);
    host_tree_->setColumnCount(2);
    host_tree_->header()->setStretchLastSection(false);
    host_tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    host_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    host_layout->addWidget(host_tree_, 1);

    stack_->addWidget(tree_page);
    stack_->addWidget(host_page);
    stack_->addWidget(editor_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(tree_, &QTreeWidget::itemClicked, this, &LocalWidget::onItemActivated);
    connect(overflow_button_, &IconButton::clicked, this, &LocalWidget::onShowMenu);
    connect(editor_, &LocalHostEditor::sig_accepted, this, &LocalWidget::returnFromEditor);

    // A long press on a host (an ungrouped host in the tree, or any host in a group) opens it for
    // editing.
    connect(tree_, &TreeWidget::sig_itemLongPressed, this, [this](QTreeWidgetItem* item)
    {
        if (item->data(0, kHostIdRole).isValid())
            editHost(item->data(0, kHostIdRole).toLongLong());
    });
    connect(host_tree_, &TreeWidget::sig_itemLongPressed, this, [this](QTreeWidgetItem* item)
    {
        editHost(item->data(0, kHostIdRole).toLongLong());
    });

    reload();
}

//--------------------------------------------------------------------------------------------------
LocalWidget::~LocalWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> LocalWidget::appBarActions() const
{
    // The editor screen has its own form; no browsing actions there.
    if (stack_->currentIndex() == kPageEditor)
        return {};

    return { search_button_, overflow_button_ };
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::reload()
{
    // Host and group names are decrypted with the master-password-derived key, so the tree is empty
    // until the cryptor is unlocked.
    if (!DataCryptor::instance().isValid())
        return;

    showTree();
    tree_->clear();

    // Top-level groups, then the ungrouped hosts (group_id 0) shown directly at the root.
    populateGroups(0, nullptr);

    const QIcon icon = GuiApplication::svgIcon(":/img/computer.svg");
    for (const HostConfig& host : Database::instance().hostList(0))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(tree_, { host.name(), host.address() });
        item->setIcon(0, icon);
        item->setData(0, kHostIdRole, host.id());
    }
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::goBack()
{
    if (stack_->currentIndex() == kPageEditor)
        returnFromEditor();
    else
        showTree();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::retranslate()
{
    reload();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onItemActivated(QTreeWidgetItem* item, int /* column */)
{
    if (!item)
        return;

    // Host rows (ungrouped hosts at the root) are not actionable yet; only groups open a list.
    if (item->data(0, kHostIdRole).isValid())
        return;

    showHosts(item->data(0, kGroupIdRole).toLongLong(), item->text(0));
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onShowMenu()
{
    Menu* menu = new Menu(this);
    menu->addItem(tr("Add Host"), GuiApplication::svgIcon(":/img/material/add_2.svg"));
    connect(menu, &Menu::sig_triggered, this, [this](int /* index */) { onAddHost(); });

    // Anchor the menu to the button; it drops from the button's near edge per layout direction.
    menu->popup(QRect(overflow_button_->mapToGlobal(QPoint(0, 0)), overflow_button_->size()));
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onAddHost()
{
    // current_group_id_ holds the target: the open group on the host page, or the root on the tree.
    editor_->prepareForAdd(current_group_id_);
    stack_->setCurrentIndex(kPageEditor);
    emit sig_titleChanged(tr("Add Host"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::editHost(qint64 host_id)
{
    editor_->prepareForEdit(host_id);
    stack_->setCurrentIndex(kPageEditor);
    emit sig_titleChanged(tr("Edit Host"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::returnFromEditor()
{
    // Return to where the editor was opened from, refreshing so a newly saved host shows up. The
    // page is switched first so appBarActions() reflects the destination when the actions update.
    if (current_group_id_ == 0)
        reload();
    else
        showHosts(current_group_id_, current_title_);

    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::populateGroups(qint64 parent_id, QTreeWidgetItem* parent)
{
    const QIcon icon = GuiApplication::svgIcon(":/img/folder.svg");

    for (const GroupConfig& group : Database::instance().groupList(parent_id))
    {
        QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent, { group.name() })
                                       : new QTreeWidgetItem(tree_, { group.name() });
        item->setIcon(0, icon);
        item->setData(0, kGroupIdRole, group.id());
        item->setExpanded(true);

        populateGroups(group.id(), item);
    }
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::showTree()
{
    // A new host added from the tree goes to the root (ungrouped).
    current_group_id_ = 0;
    current_title_.clear();

    stack_->setCurrentIndex(kPageTree);
    emit sig_titleChanged(QString(), false);
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::showHosts(qint64 group_id, const QString& title)
{
    // A new host added from this page goes to the open group.
    current_group_id_ = group_id;
    current_title_ = title;

    host_tree_->clear();

    const QIcon icon = GuiApplication::svgIcon(":/img/computer.svg");

    for (const HostConfig& host : Database::instance().hostList(group_id))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(host_tree_, { host.name(), host.address() });
        item->setIcon(0, icon);
        item->setData(0, kHostIdRole, host.id());
    }

    stack_->setCurrentIndex(kPageHosts);
    emit sig_titleChanged(title, true);
}
