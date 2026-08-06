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
#include "common/desktop/router_error.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_relays_widget.h"

//--------------------------------------------------------------------------------------------------
RouterRelaysWidget::RouterRelaysWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_RELAYS, parent),
      ui(std::make_unique<Ui::RouterRelaysWidget>()),
      status_relays_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    relay_model_ = new RelayListModel(this);
    peer_model_ = new PeerListModel(this);

    ui->tree_relays->setModel(relay_model_);
    ui->tree_peers->setModel(peer_model_);

    // Turned on again after the models are set: a view wires its header up to the sort of whatever
    // model it has, and at the time the generated setup ran there was none.
    ui->tree_relays->setSortingEnabled(true);
    ui->tree_peers->setSortingEnabled(true);

    ui->tree_relays->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_relays, &QWidget::customContextMenuRequested,
            this, &RouterRelaysWidget::onRelayContextMenu);

    ui->tree_relays->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_relays->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterRelaysWidget::onHeaderContextMenu);

    connect(ui->tree_relays->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RouterRelaysWidget::onCurrentRelayChanged);

    ui->tree_peers->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_peers, &QWidget::customContextMenuRequested,
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
                    relay_model_->clear();
                    peer_model_->clear();
                    updateStatusLabel();
                }
            });
        }
    }

    router_id_ = router_id;

    relay_model_->clear();
    peer_model_->clear();
    updateStatusLabel();
    fetchRelays();
}

//--------------------------------------------------------------------------------------------------
bool RouterRelaysWidget::hasSelectedRelay() const
{
    return currentRelay() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterRelaysWidget::relayCount() const
{
    return relay_model_->rowCount();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::copyCurrentRelayRow()
{
    const int row = ui->tree_relays->currentIndex().row();
    if (row < 0)
        return;

    QString result;
    for (int i = 0; i < relay_model_->columnCount(); ++i)
    {
        const QString text = relay_model_->index(row, i).data().toString();
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
    const int row = ui->tree_relays->currentIndex().row();
    if (row < 0 || column < 0 || column >= relay_model_->columnCount())
        return;

    const QString text = relay_model_->index(row, column).data().toString();
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

    for (int i = 0; i < relay_model_->rowCount(); ++i)
    {
        const proto::router::RelayInfo& info = *relay_model_->relayAt(i);

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
    const proto::router::RelayInfo* relay = currentRelay();
    if (!relay)
    {
        LOG(INFO) << "No selected relay";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect relay \"%1\"?")
        .arg(QString::fromStdString(relay->ip_address()))) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect relay rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect relay accepted by user";
    router->disconnectRelay(relay->entry_id(),
                            { this, &RouterRelaysWidget::onRelayResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onDisconnectAllRelays()
{
    if (relay_model_->rowCount() <= 0)
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
    router->disconnectRelay(-1, { this, &RouterRelaysWidget::onRelayResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onCurrentRelayChanged()
{
    updateRelayStatistics();
    emit sig_currentChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onRelayContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->tree_relays->indexAt(pos);
    if (index.isValid())
        ui->tree_relays->setCurrentIndex(index);

    const int column = index.column();
    emit sig_contextMenu(ui->tree_relays->viewport()->mapToGlobal(pos), column);
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onPeerContextMenu(const QPoint& pos)
{
    const proto::router::RelayInfo* relay = currentRelay();
    if (!relay)
        return;

    const QModelIndex index = ui->tree_peers->indexAt(pos);
    if (!index.isValid())
        return;

    ui->tree_peers->setCurrentIndex(index);

    const proto::router::Peer* peer = currentPeer();
    if (!peer)
        return;

    const int column = index.column();
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
                    .arg(QString::fromStdString(peer->client_user_name())))
            != MsgBox::Yes)
        {
            LOG(INFO) << "[ACTION] Disconnect peer rejected by user";
            return;
        }

        Router* router = Router::instance(router_id_);
        if (!router)
            return;

        LOG(INFO) << "[ACTION] Disconnect peer accepted by user";
        router->disconnectPeer(relay->entry_id(), peer->peer_id(),
                               { this, &RouterRelaysWidget::onPeerResultReceived });
    }
    else if (selected == copy_row_action)
    {
        QString result;
        for (int i = 0; i < peer_model_->columnCount(); ++i)
        {
            const QString text = peer_model_->index(index.row(), i).data().toString();
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
        const QString text = peer_model_->index(index.row(), column).data().toString();
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
        ColumnAction* action = new ColumnAction(
            relay_model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), i, &menu);
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
    const proto::router::RelayInfo* selected = currentRelay();
    const qint64 selected_entry_id = selected ? selected->entry_id() : 0;

    relay_model_->setRelays(relays);

    // The list is replaced whole, so the row the user was on has to be found again by the relay it
    // was showing.
    const int selected_row = relay_model_->rowOf(selected_entry_id);
    if (selected_row >= 0)
        ui->tree_relays->setCurrentIndex(relay_model_->index(selected_row, 0));

    updateRelayStatistics();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onRelayResultReceived(const proto::router::RelayResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        MsgBox::warning(this, routerErrorText(error_code));
    }

    fetchRelays();
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::onPeerResultReceived(const proto::router::PeerResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        MsgBox::warning(this, routerErrorText(error_code));
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

    router->listRelays({ this, &RouterRelaysWidget::onRelayListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::updateRelayStatistics()
{
    const proto::router::RelayInfo* relay = currentRelay();
    if (!relay)
    {
        peer_model_->clear();
        ui->tree_peers->setEnabled(false);
        return;
    }

    ui->tree_peers->setEnabled(true);

    // A relay that reports no statistics is serving nobody as far as we know, so the pairs of the
    // relay that was shown before are not left standing.
    if (!relay->has_statistics())
    {
        peer_model_->clear();
        return;
    }

    const proto::router::Peer* selected = currentPeer();
    const qint64 selected_peer_id = selected ? selected->peer_id() : 0;

    peer_model_->setPeers(relay->statistics());

    const int selected_row = peer_model_->rowOf(selected_peer_id);
    if (selected_row >= 0)
        ui->tree_peers->setCurrentIndex(peer_model_->index(selected_row, 0));
}

//--------------------------------------------------------------------------------------------------
void RouterRelaysWidget::updateStatusLabel()
{
    status_relays_label_->setText(tr("%n relay(s)", "", relay_model_->rowCount()));
}

//--------------------------------------------------------------------------------------------------
const proto::router::RelayInfo* RouterRelaysWidget::currentRelay() const
{
    return relay_model_->relayAt(ui->tree_relays->currentIndex().row());
}

//--------------------------------------------------------------------------------------------------
const proto::router::Peer* RouterRelaysWidget::currentPeer() const
{
    return peer_model_->peerAt(ui->tree_peers->currentIndex().row());
}
