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

#include "client/ui/hosts/router_group_widget.h"

#include <QAction>
#include <QCollator>
#include <QDataStream>
#include <QDateTime>
#include <QEvent>
#include <QHeaderView>
#include <QIODevice>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>

#include "base/logging.h"
#include "client/router.h"
#include "client/ui/hosts/router_host_dialog.h"
#include "proto/router_client.h"
#include "ui_router_group_widget.h"

namespace {

enum Column
{
    COLUMN_DISPLAY_NAME = 0,
    COLUMN_HOST_ID,
    COLUMN_COMPUTER_NAME,
    COLUMN_ADDRESS,
    COLUMN_USER_NAME,
    COLUMN_COMMENT,
    COLUMN_OS,
    COLUMN_VERSION,
    COLUMN_ARCH,
    COLUMN_LAST_CONNECT,
    COLUMN_LAST_MODIFY,
    COLUMN_STATUS
};

class HostTreeItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(HostTreeItem)

public:
    explicit HostTreeItem(const Router::Host& updated_host)
    {
        updateItem(updated_host);
    }

    void updateItem(const Router::Host& updated_host)
    {
        host = updated_host;

        QString name = host.display_name;
        if (name.isEmpty())
            name = host.computer_name;

        const QString last_connect = host.last_connect > 0 ? QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(host.last_connect), QLocale::ShortFormat) : QString();
        const QString last_modify = host.last_modify > 0 ? QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(host.last_modify), QLocale::ShortFormat) : QString();

        setIcon(COLUMN_DISPLAY_NAME, QIcon(host.online ? ":/img/computer-online.svg" : ":/img/computer-offline.svg"));
        setText(COLUMN_DISPLAY_NAME, name);
        setText(COLUMN_HOST_ID, QString::number(host.host_id));
        setText(COLUMN_COMPUTER_NAME, host.computer_name);
        setText(COLUMN_ADDRESS, host.address);
        setText(COLUMN_USER_NAME, host.user_name);
        setText(COLUMN_COMMENT, host.comment);
        setText(COLUMN_OS, host.os_name);
        setText(COLUMN_VERSION, host.version);
        setText(COLUMN_ARCH, host.cpu_arch);
        setText(COLUMN_LAST_CONNECT, last_connect);
        setText(COLUMN_LAST_MODIFY, last_modify);
        setText(COLUMN_STATUS, host.online ? tr("Online") : tr("Offline"));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        const int column = treeWidget()->sortColumn();
        const HostTreeItem* other_item = static_cast<const HostTreeItem*>(&other);

        if (column == COLUMN_HOST_ID)
            return host.host_id < other_item->host.host_id;
        if (column == COLUMN_LAST_CONNECT)
            return host.last_connect < other_item->host.last_connect;
        if (column == COLUMN_LAST_MODIFY)
            return host.last_modify < other_item->host.last_modify;
        if (column == COLUMN_DISPLAY_NAME)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);
            return collator.compare(text(COLUMN_DISPLAY_NAME), other.text(COLUMN_DISPLAY_NAME)) <= 0;
        }
        return QTreeWidgetItem::operator<(other);
    }

    Router::Host host;

private:
    Q_DISABLE_COPY_MOVE(HostTreeItem)
};

class ColumnAction : public QAction
{
public:
    ColumnAction(const QString& text, int index, QObject* parent)
        : QAction(text, parent),
          index_(index)
    {
        setCheckable(true);
    }

    int columnIndex() const { return index_; }

private:
    const int index_;
    Q_DISABLE_COPY_MOVE(ColumnAction)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::RouterGroupWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_GROUP, parent),
      ui(std::make_unique<Ui::RouterGroupWidget>()),
      status_hosts_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->tree_host->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tree_host->header()->setSectionHidden(COLUMN_USER_NAME, true);
    ui->tree_host->header()->setSectionHidden(COLUMN_ARCH, true);
    ui->tree_host->header()->setSectionHidden(COLUMN_COMMENT, true);
    ui->tree_host->header()->setSectionHidden(COLUMN_ADDRESS, true);

    connect(ui->tree_host->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterGroupWidget::onHeaderContextMenu);

    ui->tree_host->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_host, &QTreeWidget::customContextMenuRequested,
            this, &RouterGroupWidget::onHostContextMenu);

    connect(ui->tree_host, &QTreeWidget::itemSelectionChanged,
            this, &RouterGroupWidget::sig_currentChanged);

    connect(ui->tree_host, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem*, int) { emit sig_doubleClicked(); });
}

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::~RouterGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::showGroup(qint64 router_id, qint64 workspace_id, qint64 group_id)
{
    if (router_id_ != router_id)
    {
        // Move the sig_hostsChanged subscription to the router whose workspace is now displayed.
        if (Router* prev = Router::instance(router_id_))
            disconnect(prev, &Router::sig_hostsChanged, this, nullptr);

        if (Router* curr = Router::instance(router_id))
            connect(curr, &Router::sig_hostsChanged, this, &RouterGroupWidget::fetchHosts);
    }

    router_id_ = router_id;
    workspace_id_ = workspace_id;
    group_id_ = group_id;

    // Clear the tree before showing a different group to avoid briefly displaying stale
    // entries from the previous one.
    ui->tree_host->clear();
    updateStatusLabel();
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
bool RouterGroupWidget::hasSelectedHost() const
{
    return ui->tree_host->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
Router::Host RouterGroupWidget::selectedHost() const
{
    HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_host->currentItem());
    return item ? item->host : Router::Host();
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterGroupWidget::selectedHostConfig() const
{
    HostConfig config;

    if (!hasSelectedHost())
        return config;

    const Router::Host selected = selectedHost();
    if (selected.host_id == kInvalidHostId)
        return config;

    config.setRouterId(router_id_);
    config.setAddress(hostIdToString(selected.host_id));

    QString name = selected.display_name;
    if (name.isEmpty())
        name = selected.computer_name;
    config.setName(name);

    config.setUsername(selected.user_name);
    config.setPassword(selected.password);
    return config;
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterGroupWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_host->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
        ui->tree_host->header()->restoreState(columns_state);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::reload()
{
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabel();

    statusbar->addWidget(status_hosts_label_);
    status_hosts_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_hosts_label_);
    status_hosts_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onEditHost()
{
    HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_host->currentItem());
    if (!item)
        return;

    RouterHostDialog dialog(router_id_, item->host, this);
    if (dialog.exec() == QDialog::Accepted)
        fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onHostListReceived(const Router::HostList& list)
{
    // The router echoes workspace_id/group_id; ignore responses for other workspaces or
    // groups (e.g. an in-flight request issued before the user switched selection).
    if (list.workspace_id != workspace_id_ || list.group_id != group_id_)
        return;

    auto has_with_id = [](const Router::HostList& list, HostId host_id)
    {
        for (const Router::Host& host : std::as_const(list.hosts))
        {
            if (host.host_id == host_id)
                return true;
        }
        return false;
    };

    // Remove from the UI all hosts that are not in the list.
    for (int i = ui->tree_host->topLevelItemCount() - 1; i >= 0; --i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_host->topLevelItem(i));
        if (!has_with_id(list, item->host.host_id))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (const Router::Host& host : std::as_const(list.hosts))
    {
        bool found = false;

        for (int j = 0; j < ui->tree_host->topLevelItemCount(); ++j)
        {
            HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_host->topLevelItem(j));
            if (item->host.host_id == host.host_id)
            {
                item->updateItem(host);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_host->addTopLevelItem(new HostTreeItem(host));
    }

    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_host->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui->tree_host->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onHostContextMenu(const QPoint& pos)
{
    HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_host->itemAt(pos));
    if (!item)
        return;

    ui->tree_host->setCurrentItem(item);
    emit sig_contextMenu(ui->tree_host->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::fetchHosts()
{
    if (router_id_ == 0)
        return;

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_FILTERED);
    request.set_workspace_id(workspace_id_);
    request.set_group_id(group_id_);
    router->listHosts(std::move(request), this, &RouterGroupWidget::onHostListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::updateStatusLabel()
{
    status_hosts_label_->setText(tr("%n host(s)", "", ui->tree_host->topLevelItemCount()));
}
