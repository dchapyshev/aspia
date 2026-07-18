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
#include <QCollator>
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
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_clients_widget.h"

namespace {

class ClientTreeItem final : public QTreeWidgetItem
{
public:
    explicit ClientTreeItem(const proto::router::ClientInfo& info)
    {
        setIcon(0, QIcon(":/img/computer.svg"));
        updateItem(info);
    }

    void updateItem(const proto::router::ClientInfo& updated_info)
    {
        info = updated_info;

        QString time = QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(info.timepoint()), QLocale::ShortFormat);

        setText(0, QString::fromStdString(info.computer_name()));
        setText(1, QString::fromStdString(info.ip_address()));
        setText(2, time);

        const proto::peer::Version& version = info.version();

        setText(3, QString("%1.%2.%3").arg(version.major()).arg(version.minor()).arg(version.patch()));
        setText(4, QString::fromStdString(info.architecture()));
        setText(5, QString::fromStdString(info.os_name()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 0)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(0), other.text(0)) < 0;
        }
        else if (column == 2)
        {
            const ClientTreeItem* other_item = static_cast<const ClientTreeItem*>(&other);
            return info.timepoint() < other_item->info.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::ClientInfo info;

private:
    Q_DISABLE_COPY_MOVE(ClientTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterClientsWidget::RouterClientsWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_CLIENTS, parent),
      ui(std::make_unique<Ui::RouterClientsWidget>()),
      status_clients_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->tree_clients->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_clients, &QTreeWidget::customContextMenuRequested,
            this, &RouterClientsWidget::onClientContextMenu);

    ui->tree_clients->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_clients->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterClientsWidget::onHeaderContextMenu);

    connect(ui->tree_clients, &QTreeWidget::itemSelectionChanged,
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
                    ui->tree_clients->clear();
                    updateStatusLabel();
                }
            });
        }
    }

    router_id_ = router_id;

    ui->tree_clients->clear();
    updateStatusLabel();
    fetchClients();
}

//--------------------------------------------------------------------------------------------------
bool RouterClientsWidget::hasSelectedClient() const
{
    return ui->tree_clients->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterClientsWidget::clientCount() const
{
    return ui->tree_clients->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::copyCurrentClientRow()
{
    QTreeWidgetItem* item = ui->tree_clients->currentItem();
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
void RouterClientsWidget::copyCurrentClientColumn(int column)
{
    QTreeWidgetItem* item = ui->tree_clients->currentItem();
    if (!item || column < 0)
        return;

    const QString text = item->text(column);
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

    for (int i = 0; i < ui->tree_clients->topLevelItemCount(); ++i)
    {
        const proto::router::ClientInfo& info =
            static_cast<ClientTreeItem*>(ui->tree_clients->topLevelItem(i))->info;

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
    ClientTreeItem* tree_item = static_cast<ClientTreeItem*>(ui->tree_clients->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected client";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect client \"%1\"?")
        .arg(QString::fromStdString(tree_item->info.computer_name()))) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect client rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect client accepted by user";
    router->disconnectClient(tree_item->info.entry_id(), this, &RouterClientsWidget::onClientResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onDisconnectAllClients()
{
    if (ui->tree_clients->topLevelItemCount() <= 0)
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
    router->disconnectClient(-1, this, &RouterClientsWidget::onClientResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onClientListReceived(const proto::router::ClientList& clients)
{
    auto has_with_id = [](const proto::router::ClientList& clients, qint64 entry_id)
    {
        for (int i = 0; i < clients.client_size(); ++i)
        {
            if (clients.client(i).entry_id() == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all clients that are not in the list.
    for (int i = ui->tree_clients->topLevelItemCount() - 1; i >= 0; --i)
    {
        ClientTreeItem* item = static_cast<ClientTreeItem*>(ui->tree_clients->topLevelItem(i));

        if (!has_with_id(clients, item->info.entry_id()))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < clients.client_size(); ++i)
    {
        const proto::router::ClientInfo& info = clients.client(i);
        bool found = false;

        for (int j = 0; j < ui->tree_clients->topLevelItemCount(); ++j)
        {
            ClientTreeItem* item = static_cast<ClientTreeItem*>(ui->tree_clients->topLevelItem(j));
            if (item->info.entry_id() == info.entry_id())
            {
                item->updateItem(info);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_clients->addTopLevelItem(new ClientTreeItem(info));
    }

    emit sig_currentChanged();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onClientResultReceived(const proto::router::ClientResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid client request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidEntryId)
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    fetchClients();
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onClientContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_clients->itemAt(pos);
    if (item)
        ui->tree_clients->setCurrentItem(item);

    const int column = ui->tree_clients->indexAt(pos).column();
    emit sig_contextMenu(ui->tree_clients->viewport()->mapToGlobal(pos), column);
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_clients->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui->tree_clients->headerItem()->text(i), i, &menu);
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

    router->listClients(this, &RouterClientsWidget::onClientListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterClientsWidget::updateStatusLabel()
{
    status_clients_label_->setText(tr("%n client(s)", "", ui->tree_clients->topLevelItemCount()));
}
