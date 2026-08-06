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

#include "client/desktop/management/router_clients_widget.h"

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
#include <QStatusBar>

#include "base/logging.h"
#include "client/router.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_clients_widget.h"

//--------------------------------------------------------------------------------------------------
RouterClientsWidget::RouterClientsWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_CLIENTS, parent),
      ui(std::make_unique<Ui::RouterClientsWidget>()),
      status_clients_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    model_ = new ClientListModel(this);
    ui->tree_clients->setModel(model_);

    // Turned on again after the model is set: the view wires the header up to the sort of whatever
    // model it has, and at the time the generated setup ran there was none.
    ui->tree_clients->setSortingEnabled(true);

    ui->tree_clients->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_clients, &QWidget::customContextMenuRequested,
            this, &RouterClientsWidget::onClientContextMenu);

    ui->tree_clients->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_clients->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterClientsWidget::onHeaderContextMenu);

    connect(ui->tree_clients->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RouterClientsWidget::sig_currentChanged);
}

//--------------------------------------------------------------------------------------------------
RouterClientsWidget::~RouterClientsWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::showRouter(qint64 router_id)
{
    if (router_id_ != router_id)
    {
        // Move the data/status subscriptions to the router that is now displayed.
        if (Router* prev = Router::instance(router_id_))
            disconnect(prev, nullptr, this, nullptr);

        if (Router* curr = Router::instance(router_id))
        {
            connect(curr, &Router::sig_clientsChanged, this, &RouterClientsWidget::fetchClients);
            connect(curr, &Router::sig_statusChanged, this, [this](qint64, Router::Status status)
            {
                if (status != Router::Status::ONLINE)
                {
                    model_->clear();
                    updateStatusLabel();
                }
            });
        }
    }

    router_id_ = router_id;

    model_->clear();
    updateStatusLabel();
    fetchClients();
}

//--------------------------------------------------------------------------------------------------
bool RouterClientsWidget::hasSelectedClient() const
{
    return currentClient() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterClientsWidget::clientCount() const
{
    return model_->rowCount();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::copyCurrentClientRow()
{
    const int row = ui->tree_clients->currentIndex().row();
    if (row < 0)
        return;

    QString result;
    for (int i = 0; i < model_->columnCount(); ++i)
    {
        const QString text = model_->index(row, i).data().toString();
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
void RouterClientsWidget::copyCurrentClientColumn(int column)
{
    const int row = ui->tree_clients->currentIndex().row();
    if (row < 0 || column < 0 || column >= model_->columnCount())
        return;

    const QString text = model_->index(row, column).data().toString();
    if (text.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterClientsWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_clients->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
    {
        ui->tree_clients->header()->restoreState(columns_state);
        ui->tree_clients->header()->setSectionsClickable(true);
        ui->tree_clients->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::reload()
{
    fetchClients();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::save()
{
    LOG(INFO) << "[ACTION] Save clients to file";

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

    for (int i = 0; i < model_->rowCount(); ++i)
    {
        const proto::router::ClientInfo& info = *model_->clientAt(i);

        QJsonObject client_object;

        client_object.insert("computer_name", QString::fromStdString(info.computer_name()));
        client_object.insert("operating_system", QString::fromStdString(info.os_name()));
        client_object.insert("ip_address", QString::fromStdString(info.ip_address()));
        client_object.insert("architecture", QString::fromStdString(info.architecture()));

        QString version = QString("%1.%2.%3")
            .arg(info.version().major()).arg(info.version().minor()).arg(info.version().patch());
        client_object.insert("version", version);

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            info.timepoint()), QLocale::ShortFormat);
        client_object.insert("connect_time", time);

        root_array.append(client_object);
    }

    QJsonObject root_object;
    root_object.insert("clients", root_array);

    const QByteArray json = QJsonDocument(root_object).toJson();
    if (file.write(json) != json.size() || !file.commit())
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabel();

    statusbar->addWidget(status_clients_label_);
    status_clients_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_clients_label_);
    status_clients_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onDisconnectClient()
{
    const proto::router::ClientInfo* client = currentClient();
    if (!client)
    {
        LOG(INFO) << "No selected client";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect client \"%1\"?")
        .arg(QString::fromStdString(client->computer_name()))) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect client rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect client accepted by user";
    router->disconnectClient(client->entry_id(),
                             { this, &RouterClientsWidget::onClientResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onDisconnectAllClients()
{
    if (model_->rowCount() <= 0)
    {
        LOG(INFO) << "Client list is empty";
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to disconnect all clients?")) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect all clients rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect all clients accepted by user";
    router->disconnectClient(-1, { this, &RouterClientsWidget::onClientResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onClientListReceived(const proto::router::ClientList& clients)
{
    const proto::router::ClientInfo* selected = currentClient();
    const qint64 selected_entry_id = selected ? selected->entry_id() : 0;

    model_->setClients(clients);

    // The list is replaced whole, so the row the user was on has to be found again by the session
    // it was showing.
    const int selected_row = model_->rowOf(selected_entry_id);
    if (selected_row >= 0)
        ui->tree_clients->setCurrentIndex(model_->index(selected_row, 0));

    emit sig_currentChanged();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onClientResultReceived(const proto::router::ClientResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        MsgBox::warning(this, routerErrorText(error_code));
    }

    fetchClients();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onClientContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->tree_clients->indexAt(pos);
    if (index.isValid())
        ui->tree_clients->setCurrentIndex(index);

    const int column = index.column();
    emit sig_contextMenu(ui->tree_clients->viewport()->mapToGlobal(pos), column);
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_clients->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(
            model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::fetchClients()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    if (router->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    router->listClients({ this, &RouterClientsWidget::onClientListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::updateStatusLabel()
{
    status_clients_label_->setText(tr("%n client(s)", "", model_->rowCount()));
}

//--------------------------------------------------------------------------------------------------
const proto::router::ClientInfo* RouterClientsWidget::currentClient() const
{
    return model_->clientAt(ui->tree_clients->currentIndex().row());
}
