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

#include "client/desktop/management/router_relays_widget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QSaveFile>
#include <QSplitter>
#include <QStatusBar>

#include "base/logging.h"
#include "client/router.h"
#include "common/desktop/formatter.h"
#include "common/desktop/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_relays_widget.h"

namespace {

class RelayTreeItem final : public QTreeWidgetItem
{
public:
    explicit RelayTreeItem(const proto::router::RelayInfo& info)
    {
        updateItem(info);

        QString time = QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(info.timepoint()), QLocale::ShortFormat);

        setIcon(0, QIcon(":/img/stack.svg"));
        setText(0, QString::fromStdString(info.ip_address()));
        setText(1, time);

        const proto::peer::Version& version = info.version();

        setText(3, QString("%1.%2.%3").arg(version.major()).arg(version.minor()).arg(version.patch()));
        setText(4, QString::fromStdString(info.computer_name()));
        setText(5, QString::fromStdString(info.architecture()));
        setText(6, QString::fromStdString(info.os_name()));
    }

    void updateItem(const proto::router::RelayInfo& updated_info)
    {
        info = updated_info;
        setText(2, QString::number(info.pool_size()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final
    {
        if (treeWidget()->sortColumn() == 1)
        {
            const RelayTreeItem* other_item = static_cast<const RelayTreeItem*>(&other);
            return info.timepoint() < other_item->info.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::RelayInfo info;

private:
    Q_DISABLE_COPY_MOVE(RelayTreeItem)
};

class PeerTreeItem final : public QTreeWidgetItem
{
public:
    explicit PeerTreeItem(const proto::router::Peer& connection)
    {
        updateItem(connection);

        setIcon(0, QIcon(":/img/user.svg"));
        setText(0, QString::fromStdString(conn.client_user_name()));
        setText(1, QString::number(conn.host_id()));
        setText(2, QString::fromStdString(conn.host_address()));
        setText(3, QString::fromStdString(conn.client_address()));
    }

    void updateItem(const proto::router::Peer& connection)
    {
        conn = connection;
        setText(4, Formatter::sizeToString(conn.bytes_transferred()));
        setText(5, Formatter::delayToString(Seconds(conn.duration())));
        setText(6, Formatter::delayToString(Seconds(conn.idle_time())));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 1)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.host_id() < other_item->conn.host_id();
        }
        else if (column == 4)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.bytes_transferred() < other_item->conn.bytes_transferred();
        }
        else if (column == 5)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.duration() < other_item->conn.duration();
        }
        else if (column == 6)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.idle_time() < other_item->conn.idle_time();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::Peer conn;

private:
    Q_DISABLE_COPY_MOVE(PeerTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterRelaysWidget::RouterRelaysWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_RELAYS, parent),
      ui(std::make_unique<Ui::RouterRelaysWidget>()),
      status_relays_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->tree_relays->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_relays, &QTreeWidget::customContextMenuRequested,
            this, &RouterRelaysWidget::onRelayContextMenu);

    ui->tree_relays->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_relays->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterRelaysWidget::onHeaderContextMenu);

    connect(ui->tree_relays, &QTreeWidget::itemSelectionChanged,
            this, &RouterRelaysWidget::onCurrentRelayChanged);

    ui->tree_peers->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_peers, &QTreeWidget::customContextMenuRequested,
            this, &RouterRelaysWidget::onPeerContextMenu);
}

//--------------------------------------------------------------------------------------------------
RouterRelaysWidget::~RouterRelaysWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::showRouter(qint64 router_id)
{
    if (router_id_ != router_id)
    {
        // Move the data/status subscriptions to the router that is now displayed.
        if (Router* prev = Router::instance(router_id_))
            disconnect(prev, nullptr, this, nullptr);

        if (Router* curr = Router::instance(router_id))
        {
            connect(curr, &Router::sig_relaysChanged, this, &RouterRelaysWidget::fetchRelays);
            connect(curr, &Router::sig_statusChanged, this, [this](qint64, Router::Status status)
            {
                if (status != Router::Status::ONLINE)
                {
                    ui->tree_relays->clear();
                    ui->tree_peers->clear();
                    updateStatusLabel();
                }
            });
        }
    }

    router_id_ = router_id;

    ui->tree_relays->clear();
    ui->tree_peers->clear();
    updateStatusLabel();
    fetchRelays();
}

//--------------------------------------------------------------------------------------------------
bool RouterRelaysWidget::hasSelectedRelay() const
{
    return ui->tree_relays->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterRelaysWidget::relayCount() const
{
    return ui->tree_relays->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::copyCurrentRelayRow()
{
    QTreeWidgetItem* item = ui->tree_relays->currentItem();
    if (!item)
        return;

    QString result;
    const int column_count = item->columnCount();
    for (int i = 0; i < column_count; ++i)
    {
        const QString text = item->text(i);
        if (!text.isEmpty())
            result += text + ' ';
    }
    result.chop(1);

    if (result.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(result);
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::copyCurrentRelayColumn(int column)
{
    QTreeWidgetItem* item = ui->tree_relays->currentItem();
    if (!item || column < 0)
        return;

    const QString text = item->text(column);
    if (text.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterRelaysWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->splitter->saveState();
        stream << ui->tree_relays->header()->saveState();
        stream << ui->tree_peers->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray splitter_state;
    QByteArray relays_columns_state;
    QByteArray peers_columns_state;

    stream >> splitter_state;
    stream >> relays_columns_state;
    stream >> peers_columns_state;

    if (!splitter_state.isEmpty())
    {
        ui->splitter->restoreState(splitter_state);
    }
    else
    {
        const int side_size = height() / 2;

        QList<int> sizes;
        sizes.emplace_back(side_size);
        sizes.emplace_back(side_size);
        ui->splitter->setSizes(sizes);
    }

    if (!relays_columns_state.isEmpty())
    {
        ui->tree_relays->header()->restoreState(relays_columns_state);
        ui->tree_relays->header()->setSectionsClickable(true);
        ui->tree_relays->header()->setSortIndicatorShown(true);
    }

    if (!peers_columns_state.isEmpty())
    {
        ui->tree_peers->header()->restoreState(peers_columns_state);
        ui->tree_peers->header()->setSectionsClickable(true);
        ui->tree_peers->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::reload()
{
    fetchRelays();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::save()
{
    LOG(INFO) << "[ACTION] Save relays to file";

    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("Save File"), QString(), tr("JSON files (*.json)"), &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "No selected path";
        return;
    }

    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(INFO) << "Unable to open file:" << file.errorString();
        MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < ui->tree_relays->topLevelItemCount(); ++i)
    {
        const proto::router::RelayInfo& info =
            static_cast<RelayTreeItem*>(ui->tree_relays->topLevelItem(i))->info;

        QJsonObject relay_object;

        relay_object.insert("computer_name", QString::fromStdString(info.computer_name()));
        relay_object.insert("operating_system", QString::fromStdString(info.os_name()));
        relay_object.insert("ip_address", QString::fromStdString(info.ip_address()));
        relay_object.insert("architecture", QString::fromStdString(info.architecture()));

        QString version = QString("%1.%2.%3")
            .arg(info.version().major()).arg(info.version().minor()).arg(info.version().patch());
        relay_object.insert("version", version);

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            info.timepoint()), QLocale::ShortFormat);
        relay_object.insert("connect_time", time);

        relay_object.insert("pool_size", QString::number(info.pool_size()));

        if (info.has_statistics())
        {
            const proto::router::RelayInfo::Statistics& stats = info.statistics();

            QJsonArray active_array;
            for (int j = 0; j < stats.peer_size(); ++j)
            {
                const proto::router::Peer& conn = stats.peer(j);

                QJsonObject conn_object;
                conn_object.insert("host_address", QString::fromStdString(conn.host_address()));
                conn_object.insert("host_id", QString::number(conn.host_id()));
                conn_object.insert("client_address", QString::fromStdString(conn.client_address()));
                conn_object.insert("user_name", QString::fromStdString(conn.client_user_name()));
                conn_object.insert("bytes_transferred", static_cast<long long>(conn.bytes_transferred()));
                conn_object.insert("duration", static_cast<long long>(conn.duration()));
                conn_object.insert("idle", static_cast<long long>(conn.idle_time()));

                active_array.append(conn_object);
            }

            relay_object.insert("connections", active_array);
            relay_object.insert("uptime", static_cast<long long>(stats.uptime()));
        }

        root_array.append(relay_object);
    }

    QJsonObject root_object;
    root_object.insert("relays", root_array);

    const QByteArray json = QJsonDocument(root_object).toJson();
    if (file.write(json) != json.size() || !file.commit())
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabel();

    statusbar->addWidget(status_relays_label_);
    status_relays_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_relays_label_);
    status_relays_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onDisconnectRelay()
{
    RelayTreeItem* tree_item = static_cast<RelayTreeItem*>(ui->tree_relays->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected relay";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect relay \"%1\"?")
        .arg(QString::fromStdString(tree_item->info.ip_address()))) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect relay rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect relay accepted by user";
    router->disconnectRelay(tree_item->info.entry_id(), this, &RouterRelaysWidget::onRelayResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onDisconnectAllRelays()
{
    if (ui->tree_relays->topLevelItemCount() <= 0)
    {
        LOG(INFO) << "Relay list is empty";
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to disconnect all relays?")) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect all relays rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect all relays accepted by user";
    router->disconnectRelay(-1, this, &RouterRelaysWidget::onRelayResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onCurrentRelayChanged()
{
    ui->tree_peers->clear();
    updateRelayStatistics();
    emit sig_currentChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onRelayContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_relays->itemAt(pos);
    if (item)
        ui->tree_relays->setCurrentItem(item);

    const int column = ui->tree_relays->indexAt(pos).column();
    emit sig_contextMenu(ui->tree_relays->viewport()->mapToGlobal(pos), column);
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onPeerContextMenu(const QPoint& pos)
{
    RelayTreeItem* relay_item = static_cast<RelayTreeItem*>(ui->tree_relays->currentItem());
    if (!relay_item)
        return;

    QTreeWidgetItem* item = ui->tree_peers->itemAt(pos);
    if (!item)
        return;

    ui->tree_peers->setCurrentItem(item);

    PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(item);
    const int column = ui->tree_peers->indexAt(pos).column();
    const QPoint global_pos = ui->tree_peers->viewport()->mapToGlobal(pos);

    QMenu menu;
    QAction* disconnect_action = menu.addAction(tr("Disconnect"));
    menu.addSeparator();
    QAction* copy_row_action = menu.addAction(tr("Copy Row"));
    QAction* copy_value_action = menu.addAction(tr("Copy Value"));

    QAction* selected = menu.exec(global_pos);
    if (!selected)
        return;

    if (selected == disconnect_action)
    {
        if (MsgBox::question(this,
                tr("Are you sure you want to disconnect peer \"%1\"?")
                    .arg(QString::fromStdString(peer_item->conn.client_user_name())))
            != MsgBox::Yes)
        {
            LOG(INFO) << "[ACTION] Disconnect peer rejected by user";
            return;
        }

        Router* router = Router::instance(router_id_);
        if (!router)
            return;

        LOG(INFO) << "[ACTION] Disconnect peer accepted by user";
        router->disconnectPeer(relay_item->info.entry_id(), peer_item->conn.peer_id(),
            this, &RouterRelaysWidget::onPeerResultReceived);
    }
    else if (selected == copy_row_action)
    {
        QString result;
        const int column_count = peer_item->columnCount();
        for (int i = 0; i < column_count; ++i)
        {
            const QString text = peer_item->text(i);
            if (!text.isEmpty())
                result += text + ' ';
        }
        result.chop(1);

        if (result.isEmpty())
            return;

        if (QClipboard* clipboard = QApplication::clipboard())
            clipboard->setText(result);
    }
    else if (selected == copy_value_action)
    {
        const QString text = peer_item->text(column);
        if (text.isEmpty())
            return;

        if (QClipboard* clipboard = QApplication::clipboard())
            clipboard->setText(text);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_relays->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui->tree_relays->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onRelayListReceived(const proto::router::RelayList& relays)
{
    auto has_with_id = [](const proto::router::RelayList& relays, qint64 entry_id)
    {
        for (int i = 0; i < relays.relay_size(); ++i)
        {
            if (relays.relay(i).entry_id() == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all relays that are not in the list.
    for (int i = ui->tree_relays->topLevelItemCount() - 1; i >= 0; --i)
    {
        RelayTreeItem* item = static_cast<RelayTreeItem*>(ui->tree_relays->topLevelItem(i));

        if (!has_with_id(relays, item->info.entry_id()))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < relays.relay_size(); ++i)
    {
        const proto::router::RelayInfo& info = relays.relay(i);
        bool found = false;

        for (int j = 0; j < ui->tree_relays->topLevelItemCount(); ++j)
        {
            RelayTreeItem* item = static_cast<RelayTreeItem*>(ui->tree_relays->topLevelItem(j));
            if (item->info.entry_id() == info.entry_id())
            {
                item->updateItem(info);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_relays->addTopLevelItem(new RelayTreeItem(info));
    }

    updateRelayStatistics();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onRelayResultReceived(const proto::router::RelayResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid relay request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidEntryId)
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    fetchRelays();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onPeerResultReceived(const proto::router::PeerResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorNotFound)
            message = QT_TR_NOOP("Relay session not found.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::fetchRelays()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    if (router->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    router->listRelays(this, &RouterRelaysWidget::onRelayListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::updateRelayStatistics()
{
    RelayTreeItem* item = static_cast<RelayTreeItem*>(ui->tree_relays->currentItem());
    if (!item)
    {
        ui->tree_peers->setEnabled(false);
        return;
    }

    ui->tree_peers->setEnabled(true);

    if (!item->info.has_statistics())
        return;

    const proto::router::RelayInfo::Statistics& stats = item->info.statistics();

    auto has_with_id = [](const proto::router::RelayInfo::Statistics& stats, qint64 peer_id)
    {
        for (int i = 0; i < stats.peer_size(); ++i)
        {
            if (stats.peer(i).peer_id() == peer_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all connections that are not in the list.
    for (int i = ui->tree_peers->topLevelItemCount() - 1; i >= 0; --i)
    {
        PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(ui->tree_peers->topLevelItem(i));
        if (!has_with_id(stats, peer_item->conn.peer_id()))
            delete peer_item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < stats.peer_size(); ++i)
    {
        const proto::router::Peer& connection = stats.peer(i);
        bool found = false;

        for (int j = 0; j < ui->tree_peers->topLevelItemCount(); ++j)
        {
            PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(ui->tree_peers->topLevelItem(j));
            if (peer_item->conn.peer_id() == connection.peer_id())
            {
                peer_item->updateItem(connection);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_peers->addTopLevelItem(new PeerTreeItem(connection));
    }
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::updateStatusLabel()
{
    status_relays_label_->setText(tr("%n relay(s)", "", ui->tree_relays->topLevelItemCount()));
}
