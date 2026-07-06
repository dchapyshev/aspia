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

#include <QFileDialog>
#include <QHeaderView>
#include <QStackedWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "base/crypto/data_cryptor.h"
#include "base/gui_application.h"
#include "client/android/local_group_editor.h"
#include "client/android/local_host_editor.h"
#include "client/android/password_dialog.h"
#include "client/config.h"
#include "client/database.h"
#include "client/json_backup.h"
#include "client/android/search_widget.h"
#include "client/online_checker/online_checker.h"
#include "common/android/bottom_sheet.h"
#include "common/android/icon_button.h"
#include "common/android/menu.h"
#include "common/android/message_dialog.h"
#include "common/android/tree_widget.h"
#include "proto/peer.h"

namespace {

constexpr int kPageTree = 0;
constexpr int kPageHosts = 1;
constexpr int kPageHostEditor = 2;
constexpr int kPageGroupEditor = 3;
constexpr int kPageSearch = 4;

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
      group_editor_(new LocalGroupEditor(this)),
      host_editor_(new LocalHostEditor(this)),
      search_page_(new SearchWidget(this)),
      search_button_(new IconButton(":/img/material/search.svg", this)),
      refresh_button_(new IconButton(":/img/material/refresh.svg", this)),
      overflow_button_(new IconButton(":/img/material/more_vert.svg", this)),
      online_checker_(new OnlineChecker(this))
{
    // The action buttons live in the app bar; AppBar::setActions() reparents and shows the ones it
    // receives. Hidden by default so they do not linger in this widget.
    search_button_->hide();
    refresh_button_->hide();
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
    stack_->addWidget(host_editor_);
    stack_->addWidget(group_editor_);
    stack_->addWidget(search_page_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(tree_, &QTreeWidget::itemClicked, this, &LocalWidget::onItemActivated);

    // A tap on a host opens the session-type chooser. The group tree is routed through
    // onItemActivated (it must also open groups); the host list has only hosts.
    connect(host_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        if (item && item->data(0, kHostIdRole).isValid())
            showSessionMenu(item->data(0, kHostIdRole).toLongLong());
    });

    connect(search_button_, &IconButton::clicked, this, &LocalWidget::showSearch);
    connect(refresh_button_, &IconButton::clicked, this, &LocalWidget::onRefreshClicked);
    connect(search_page_, &SearchWidget::sig_activated, this, [this](const QVariant& data)
    {
        showSessionMenu(data.toLongLong());
    });
    connect(overflow_button_, &IconButton::clicked, this, &LocalWidget::onShowMenu);
    connect(host_editor_, &LocalHostEditor::sig_accepted, this, &LocalWidget::returnFromEditor);
    connect(group_editor_, &LocalGroupEditor::sig_accepted, this, &LocalWidget::returnFromEditor);

    // A long press opens the row for editing: an ungrouped host in the tree, a group in the tree,
    // or a host inside an opened group.
    connect(tree_, &TreeWidget::sig_itemLongPressed, this, [this](QTreeWidgetItem* item)
    {
        if (item->data(0, kHostIdRole).isValid())
            editHost(item->data(0, kHostIdRole).toLongLong());
        else
            editGroup(item->data(0, kGroupIdRole).toLongLong());
    });
    connect(host_tree_, &TreeWidget::sig_itemLongPressed, this, [this](QTreeWidgetItem* item)
    {
        editHost(item->data(0, kHostIdRole).toLongLong());
    });

    connect(online_checker_, &OnlineChecker::sig_checkerResult,
            this, &LocalWidget::onOnlineCheckerResult);

    reload();
}

//--------------------------------------------------------------------------------------------------
LocalWidget::~LocalWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> LocalWidget::appBarActions() const
{
    // The editor and search screens have their own forms; no browsing actions there.
    if (isEditorPage() || isSearchPage())
        return {};

    return { search_button_, refresh_button_, overflow_button_ };
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

    OnlineChecker::HostList hosts;
    const QIcon icon = GuiApplication::svgIcon(":/img/computer.svg");
    for (const HostConfig& host : Database::instance().hostList(0))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(tree_, { host.name(), host.address() });
        item->setIcon(0, icon);
        item->setData(0, kHostIdRole, host.id());
        hosts.append(host);
    }

    online_checker_->start(hosts);
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::goBack()
{
    if (isEditorPage())
    {
        returnFromEditor();
    }
    else if (isSearchPage())
    {
        showTree();
        emit sig_searchModeChanged(false);
    }
    else
    {
        showTree();
    }
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::showSearch()
{
    search_page_->reset();
    stack_->setCurrentIndex(kPageSearch);
    emit sig_searchModeChanged(true);
}

//--------------------------------------------------------------------------------------------------
bool LocalWidget::isSearchPage() const
{
    return stack_->currentIndex() == kPageSearch;
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::searchQuery(const QString& query)
{
    if (query.isEmpty())
    {
        search_page_->setResults({}, QString());
        return;
    }

    QList<SearchWidget::Result> results;
    for (const HostConfig& host : Database::instance().searchHosts(query))
    {
        SearchWidget::Result result;
        result.title = host.name();
        result.subtitle = host.address();
        result.icon_file_path = ":/img/computer.svg";
        result.data = host.id();
        results.append(result);
    }

    search_page_->setResults(results, query);
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onItemActivated(QTreeWidgetItem* item, int /* column */)
{
    if (!item)
        return;

    // A tap on a host row opens the session-type chooser; on a group row it opens the host list.
    if (item->data(0, kHostIdRole).isValid())
    {
        showSessionMenu(item->data(0, kHostIdRole).toLongLong());
        return;
    }

    showHosts(item->data(0, kGroupIdRole).toLongLong(), item->text(0));
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onShowMenu()
{
    enum { kAddGroup, kAddHost, kImport, kExport };

    Menu* menu = new Menu(this);
    menu->addItem(tr("Add Group"), ":/img/material/create_new_folder.svg");
    menu->addItem(tr("Add Host"), ":/img/material/add_2.svg");
    menu->addItem(tr("Import"), ":/img/material/download.svg");
    menu->addItem(tr("Export"), ":/img/material/upload.svg");

    connect(menu, &Menu::sig_triggered, this, [this](int index)
    {
        switch (index)
        {
            case kImport: onImport(); break;
            case kExport: onExport(); break;
            case kAddGroup: onAddGroup(); break;
            case kAddHost: onAddHost(); break;
            default: break;
        }
    });

    // Anchor the menu to the button; it drops from the button's near edge per layout direction.
    menu->popup(QRect(overflow_button_->mapToGlobal(QPoint(0, 0)), overflow_button_->size()));
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onImport()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Address Book"), QString(), tr("Address book (*.json)"));
    if (path.isEmpty())
        return;

    PasswordDialog dialog(PasswordDialog::Mode::ENTER, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    JsonBackup::ImportCounts counts;
    switch (JsonBackup::importFromFile(path, dialog.password(), &counts))
    {
        case JsonBackup::Result::SUCCESS:
            reload();
            MessageDialog::info(this, tr("Import"),
                tr("Imported %n router(s), ", nullptr, counts.routers) +
                tr("%n group(s), ", nullptr, counts.groups) +
                tr("%n host(s).", nullptr, counts.hosts));
            break;

        case JsonBackup::Result::WRONG_PASSWORD:
            MessageDialog::info(this, tr("Import"), tr("Invalid password."));
            break;

        case JsonBackup::Result::UNSUPPORTED_VERSION:
            MessageDialog::info(this, tr("Import"),
                tr("The file was created by a newer version and cannot be imported."));
            break;

        case JsonBackup::Result::NOTHING_IMPORTED:
            MessageDialog::info(this, tr("Import"), tr("The address book is already up to date."));
            break;

        default:
            MessageDialog::info(this, tr("Import"), tr("Failed to import the address book."));
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onExport()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Address Book"), "aspia.json", tr("Address book (*.json)"));
    if (path.isEmpty())
        return;

    PasswordDialog dialog(PasswordDialog::Mode::SET, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    JsonBackup::ExportCounts counts;
    if (JsonBackup::exportToFile(path, dialog.password(), &counts) == JsonBackup::Result::SUCCESS)
    {
        MessageDialog::info(this, tr("Export"),
            tr("Exported %n router(s), ", nullptr, counts.routers) +
            tr("%n group(s), ", nullptr, counts.groups) +
            tr("%n host(s).", nullptr, counts.hosts));
    }
    else
    {
        MessageDialog::info(this, tr("Export"), tr("Failed to export the address book."));
    }
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onAddGroup()
{
    // current_group_id_ holds the target: the open group on the host page, or the root on the tree.
    group_editor_->prepareForAdd(current_group_id_);
    stack_->setCurrentIndex(kPageGroupEditor);
    emit sig_titleChanged(tr("Add Group"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onAddHost()
{
    // current_group_id_ holds the target: the open group on the host page, or the root on the tree.
    host_editor_->prepareForAdd(current_group_id_);
    stack_->setCurrentIndex(kPageHostEditor);
    emit sig_titleChanged(tr("Add Host"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onOnlineCheckerResult(qint64 entry_id, bool online)
{
    const QIcon icon = GuiApplication::svgIcon(
        online ? ":/img/computer-online.svg" : ":/img/computer-offline.svg");

    // The host may be shown in the root tree (ungrouped) or in an opened group's list.
    for (TreeWidget* tree : { tree_, host_tree_ })
    {
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = tree->topLevelItem(i);
            const QVariant host_id = item->data(0, kHostIdRole);
            if (host_id.isValid() && host_id.toLongLong() == entry_id)
                item->setIcon(0, icon);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::onRefreshClicked()
{
    // Recheck the hosts visible on the current page: the open group's list, or the ungrouped hosts
    // at the root. invalidate() drops their cached status so start() issues a fresh probe.
    const qint64 group_id = (stack_->currentIndex() == kPageHosts) ? current_group_id_ : 0;

    QList<qint64> entry_ids;
    OnlineChecker::HostList hosts;
    for (const HostConfig& host : Database::instance().hostList(group_id))
    {
        entry_ids.append(host.id());
        hosts.append(host);
    }

    online_checker_->invalidate(entry_ids);

    // Reset the probed hosts to the neutral icon before starting, so it is visible that the statuses
    // are being refreshed instead of leaving the stale online/offline marks during the recheck.
    const QIcon neutral_icon = GuiApplication::svgIcon(":/img/computer.svg");
    for (TreeWidget* tree : { tree_, host_tree_ })
    {
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = tree->topLevelItem(i);
            const QVariant host_id = item->data(0, kHostIdRole);
            if (host_id.isValid() && entry_ids.contains(host_id.toLongLong()))
                item->setIcon(0, neutral_icon);
        }
    }

    online_checker_->start(hosts);
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::editGroup(qint64 group_id)
{
    group_editor_->prepareForEdit(group_id);
    stack_->setCurrentIndex(kPageGroupEditor);
    emit sig_titleChanged(tr("Edit Group"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::editHost(qint64 host_id)
{
    host_editor_->prepareForEdit(host_id);
    stack_->setCurrentIndex(kPageHostEditor);
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
bool LocalWidget::isEditorPage() const
{
    const int page = stack_->currentIndex();
    return page == kPageHostEditor || page == kPageGroupEditor;
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

    OnlineChecker::HostList hosts;
    const QIcon icon = GuiApplication::svgIcon(":/img/computer.svg");

    for (const HostConfig& host : Database::instance().hostList(group_id))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(host_tree_, { host.name(), host.address() });
        item->setIcon(0, icon);
        item->setData(0, kHostIdRole, host.id());
        hosts.append(host);
    }

    online_checker_->start(hosts);

    stack_->setCurrentIndex(kPageHosts);
    emit sig_titleChanged(title, true);
}

//--------------------------------------------------------------------------------------------------
void LocalWidget::showSessionMenu(qint64 host_id)
{
    static const struct
    {
        proto::peer::SessionType type;
        const char* name;
        const char* icon;
    } kSessions[] =
    {
        { proto::peer::SESSION_TYPE_DESKTOP,       QT_TRANSLATE_NOOP("LocalWidget", "Desktop"),
          ":/img/workstation.svg" },
        { proto::peer::SESSION_TYPE_FILE_TRANSFER, QT_TRANSLATE_NOOP("LocalWidget", "File Transfer"),
          ":/img/file-explorer.svg" },
        { proto::peer::SESSION_TYPE_CHAT,          QT_TRANSLATE_NOOP("LocalWidget", "Chat"),
          ":/img/chat.svg" },
    };

    BottomSheet* sheet = new BottomSheet(this);
    for (const auto& session : kSessions)
        sheet->addItem(tr(session.name), session.icon, false, false);

    connect(sheet, &BottomSheet::sig_triggered, this, [this, host_id](int index)
    {
        emit sig_connectHost(host_id, kSessions[index].type);
    });

    sheet->showSheet();
}
