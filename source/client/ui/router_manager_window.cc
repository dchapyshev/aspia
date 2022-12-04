//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/router_manager_window.h"

#include "base/logging.h"
#include "base/peer/user.h"
#include "qt_base/application.h"
#include "client/router.h"
#include "client/router_proxy.h"
#include "client/router_window_proxy.h"
#include "client/ui/router_user_dialog.h"
#include "client/ui/router_status_dialog.h"
#include "ui_router_manager_window.h"

#include <QDateTime>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>

namespace client {

namespace {

class HostTreeItem : public QTreeWidgetItem
{
public:
    explicit HostTreeItem(const proto::Session& session)
        : session(session)
    {
        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            static_cast<uint>(session.timepoint())), QLocale::ShortFormat);

        setText(0, QString::fromStdString(session.computer_name()));
        setText(1, QString::fromStdString(session.ip_address()));
        setText(2, time);
    }

    ~HostTreeItem() override = default;

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const override
    {
        if (treeWidget()->sortColumn() == 2)
        {
            const HostTreeItem* other_item = static_cast<const HostTreeItem*>(&other);
            return session.timepoint() < other_item->session.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::Session session;

private:
    DISALLOW_COPY_AND_ASSIGN(HostTreeItem);
};

class RelayTreeItem : public QTreeWidgetItem
{
public:
    explicit RelayTreeItem(const proto::Session& session)
        : session(session)
    {
        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            static_cast<uint>(session.timepoint())), QLocale::ShortFormat);

        setText(0, QString::fromStdString(session.ip_address()));
        setText(1, time);

        proto::RelaySessionData session_data;
        if (session_data.ParseFromString(session.session_data()))
        {
            setText(2, QString::number(session_data.pool_size()));
        }

        const proto::Version& version = session.version();
        
        setText(3, QString("%1.%2.%3.%4")
            .arg(version.major()).arg(version.minor()).arg(version.patch()).arg(version.revision()));
        setText(4, QString::fromStdString(session.computer_name()));
        setText(5, QString::fromStdString(session.os_name()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const override
    {
        if (treeWidget()->sortColumn() == 1)
        {
            const RelayTreeItem* other_item = static_cast<const RelayTreeItem*>(&other);
            return session.timepoint() < other_item->session.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::Session session;

private:
    DISALLOW_COPY_AND_ASSIGN(RelayTreeItem);
};

class UserTreeItem : public QTreeWidgetItem
{
public:
    explicit UserTreeItem(const proto::User& user)
        : user(base::User::parseFrom(user))
    {
        setText(0, QString::fromStdString(user.name()));

        if (user.flags() & base::User::ENABLED)
            setIcon(0, QIcon(QStringLiteral(":/img/user.png")));
        else
            setIcon(0, QIcon(QStringLiteral(":/img/user-disabled.png")));
    }

    base::User user;

private:
    DISALLOW_COPY_AND_ASSIGN(UserTreeItem);
};

} // namespace

RouterManagerWindow::RouterManagerWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(std::make_unique<Ui::RouterManagerWindow>()),
      status_dialog_(new RouterStatusDialog(this)),
      window_proxy_(std::make_shared<RouterWindowProxy>(
          qt_base::Application::uiTaskRunner(), this))
{
    ui->setupUi(this);

    connect(ui->button_refresh_hosts, &QPushButton::clicked,
            this, &RouterManagerWindow::refreshSessionList);

    connect(ui->button_disconnect_host, &QPushButton::clicked,
            this, &RouterManagerWindow::disconnectHost);

    connect(ui->button_disconnect_all_hosts, &QPushButton::clicked,
            this, &RouterManagerWindow::disconnectAllHosts);

    connect(ui->button_refresh_relay, &QPushButton::clicked,
            this, &RouterManagerWindow::refreshSessionList);

    connect(ui->button_refresh_users, &QPushButton::clicked,
            this, &RouterManagerWindow::refreshUserList);

    connect(ui->button_add_user, &QPushButton::clicked,
            this, &RouterManagerWindow::addUser);

    connect(ui->button_modify_user, &QPushButton::clicked,
            this, &RouterManagerWindow::modifyUser);

    connect(ui->button_delete_user, &QPushButton::clicked,
            this, &RouterManagerWindow::deleteUser);

    connect(ui->tree_users, &QTreeWidget::currentItemChanged,
            this, &RouterManagerWindow::onCurrentUserChanged);

    connect(ui->tree_hosts, &QTreeWidget::currentItemChanged,
            this, &RouterManagerWindow::onCurrentHostChanged);

    ui->tree_hosts->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_hosts, &QTreeWidget::customContextMenuRequested,
            this, &RouterManagerWindow::onHostsContextMenu);

    ui->tree_relay->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_relay, &QTreeWidget::customContextMenuRequested,
            this, &RouterManagerWindow::onRelaysContextMenu);

    connect(ui->button_save_hosts, &QPushButton::clicked,
            this, &RouterManagerWindow::saveHostsToFile);

    connect(ui->button_save_relays, &QPushButton::clicked,
            this, &RouterManagerWindow::saveRelaysToFile);
}

RouterManagerWindow::~RouterManagerWindow()
{
    window_proxy_->dettach();
}

void RouterManagerWindow::connectToRouter(const RouterConfig& router_config)
{
    peer_address_ = QString::fromStdU16String(router_config.address);
    peer_port_ = router_config.port;

    status_dialog_->setStatus(tr("Connecting to %1:%2...").arg(peer_address_).arg(peer_port_));
    status_dialog_->show();
    status_dialog_->activateWindow();

    connect(status_dialog_, &RouterStatusDialog::canceled, [this]()
    {
        router_proxy_.reset();
        close();
    });

    std::unique_ptr<Router> router = std::make_unique<Router>(
        window_proxy_, qt_base::Application::ioTaskRunner());

    router->setUserName(router_config.username);
    router->setPassword(router_config.password);

    router_proxy_ = std::make_unique<RouterProxy>(
        qt_base::Application::ioTaskRunner(), std::move(router));

    router_proxy_->connectToRouter(router_config.address, router_config.port);
}

void RouterManagerWindow::onConnected(const base::Version& peer_version)
{
    status_dialog_->hide();

    ui->statusbar->showMessage(tr("Connected to: %1:%2 (version %3)")
                               .arg(peer_address_)
                               .arg(peer_port_)
                               .arg(QString::fromStdString(peer_version.toString())));
    show();
    activateWindow();

    if (router_proxy_)
    {
        router_proxy_->refreshSessionList();
        router_proxy_->refreshUserList();
    }
}

void RouterManagerWindow::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case base::NetworkChannel::ErrorCode::INVALID_PROTOCOL:
            message = QT_TR_NOOP("Violation of the communication protocol.");
            break;

        case base::NetworkChannel::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("Cryptography error (message encryption or decryption failed).");
            break;

        case base::NetworkChannel::ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("An error occurred with the network (e.g., the network cable was accidentally plugged out).");
            break;

        case base::NetworkChannel::ErrorCode::CONNECTION_REFUSED:
            message = QT_TR_NOOP("Connection was refused by the peer (or timed out).");
            break;

        case base::NetworkChannel::ErrorCode::REMOTE_HOST_CLOSED:
            message = QT_TR_NOOP("Remote host closed the connection.");
            break;

        case base::NetworkChannel::ErrorCode::SPECIFIED_HOST_NOT_FOUND:
            message = QT_TR_NOOP("Host address was not found.");
            break;

        case base::NetworkChannel::ErrorCode::SOCKET_TIMEOUT:
            message = QT_TR_NOOP("Socket operation timed out.");
            break;

        case base::NetworkChannel::ErrorCode::ADDRESS_IN_USE:
            message = QT_TR_NOOP("Address specified is already in use and was set to be exclusive.");
            break;

        case base::NetworkChannel::ErrorCode::ADDRESS_NOT_AVAILABLE:
            message = QT_TR_NOOP("Address specified does not belong to the host.");
            break;

        default:
        {
            if (error_code != base::NetworkChannel::ErrorCode::UNKNOWN)
            {
                LOG(LS_WARNING) << "Unknown error code: " << static_cast<int>(error_code);
            }

            message = QT_TR_NOOP("An unknown error occurred.");
        }
        break;
    }

    status_dialog_->setStatus(tr("Error: %1").arg(tr(message)));
    status_dialog_->show();
}

void RouterManagerWindow::onAccessDenied(base::ClientAuthenticator::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case base::ClientAuthenticator::ErrorCode::SUCCESS:
            message = QT_TR_NOOP("Authentication successfully completed.");
            break;

        case base::ClientAuthenticator::ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("Network authentication error.");
            break;

        case base::ClientAuthenticator::ErrorCode::PROTOCOL_ERROR:
            message = QT_TR_NOOP("Violation of the data exchange protocol.");
            break;

        case base::ClientAuthenticator::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("An error occured while authenticating: wrong user name or password.");
            break;

        case base::ClientAuthenticator::ErrorCode::SESSION_DENIED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        default:
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    status_dialog_->setStatus(tr("Error: %1").arg(tr(message)));
    status_dialog_->show();
}

void RouterManagerWindow::onSessionList(std::shared_ptr<proto::SessionList> session_list)
{
    QTreeWidget* tree_hosts = ui->tree_hosts;
    QTreeWidget* tree_relay = ui->tree_relay;

    tree_hosts->clear();
    tree_relay->clear();

    int host_count = 0;
    int relay_count = 0;

    for (int i = 0; i < session_list->session_size(); ++i)
    {
        const proto::Session& session = session_list->session(i);

        switch (session.session_type())
        {
            case proto::ROUTER_SESSION_HOST:
            {
                tree_hosts->addTopLevelItem(new HostTreeItem(session_list->session(i)));
                ++host_count;
            }
            break;

            case proto::ROUTER_SESSION_RELAY:
            {
                tree_relay->addTopLevelItem(new RelayTreeItem(session_list->session(i)));
                ++relay_count;
            }
            break;

            default:
                break;
        }
    }

    ui->label_hosts_conn_count->setText(QString::number(host_count));
    ui->label_relay_conn_count->setText(QString::number(relay_count));

    for (int i = 0; i < tree_hosts->columnCount(); ++i)
        tree_hosts->resizeColumnToContents(i);

    for (int i = 0; i < tree_relay->columnCount(); ++i)
        tree_relay->resizeColumnToContents(i);

    afterRequest();
}

void RouterManagerWindow::onSessionResult(std::shared_ptr<proto::SessionResult> session_result)
{
    if (session_result->error_code() != proto::SessionResult::SUCCESS)
    {
        const char* message;

        switch (session_result->error_code())
        {
            case proto::SessionResult::INVALID_REQUEST:
                message = QT_TR_NOOP("Invalid request.");
                break;

            case proto::SessionResult::INTERNAL_ERROR:
                message = QT_TR_NOOP("Unknown internal error.");
                break;

            case proto::SessionResult::INVALID_SESSION_ID:
                message = QT_TR_NOOP("Invalid session ID was passed.");
                break;

            default:
                message = QT_TR_NOOP("Unknown error type.");
                break;
        }

        QMessageBox::warning(this, tr("Warning"), tr(message), QMessageBox::Ok);
    }

    refreshSessionList();
    afterRequest();
}

void RouterManagerWindow::onUserList(std::shared_ptr<proto::UserList> user_list)
{
    QTreeWidget* tree_users = ui->tree_users;
    tree_users->clear();

    for (int i = 0; i < user_list->user_size(); ++i)
        tree_users->addTopLevelItem(new UserTreeItem(user_list->user(i)));

    afterRequest();
}

void RouterManagerWindow::onUserResult(std::shared_ptr<proto::UserResult> user_result)
{
    if (user_result->error_code() != proto::UserResult::SUCCESS)
    {
        const char* message;

        switch (user_result->error_code())
        {
            case proto::UserResult::INTERNAL_ERROR:
                message = QT_TR_NOOP("Unknown internal error.");
                break;

            case proto::UserResult::INVALID_DATA:
                message = QT_TR_NOOP("Invalid data was passed.");
                break;

            case proto::UserResult::ALREADY_EXISTS:
                message = QT_TR_NOOP("A user with the specified name already exists.");
                break;

            default:
                message = QT_TR_NOOP("Unknown error type.");
                break;
        }

        QMessageBox::warning(this, tr("Warning"), tr(message), QMessageBox::Ok);
    }

    refreshUserList();
    afterRequest();
}

void RouterManagerWindow::closeEvent(QCloseEvent* /* event */)
{
    if (router_proxy_)
        router_proxy_->disconnectFromRouter();
}

void RouterManagerWindow::onHostsContextMenu(const QPoint& pos)
{
    QMenu menu;
    QAction* save_action = menu.addAction(tr("Save to file..."));

    QAction* action = menu.exec(ui->tree_hosts->viewport()->mapToGlobal(pos));
    if (action == save_action)
    {
        saveHostsToFile();
    }
}

void RouterManagerWindow::onRelaysContextMenu(const QPoint& pos)
{
    QMenu menu;
    QAction* save_action = menu.addAction(tr("Save to file..."));

    QAction* action = menu.exec(ui->tree_relay->viewport()->mapToGlobal(pos));
    if (action == save_action)
    {
        saveRelaysToFile();
    }
}

void RouterManagerWindow::refreshSessionList()
{
    if (router_proxy_)
    {
        beforeRequest();
        router_proxy_->refreshSessionList();
    }
}

void RouterManagerWindow::disconnectHost()
{
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!tree_item)
        return;

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            tr("Are you sure you want to disconnect session \"%1\"?")
                                .arg(QString::fromStdString(tree_item->session.computer_name())),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::No)
    {
        return;
    }

    if (router_proxy_)
    {
        beforeRequest();
        router_proxy_->stopSession(tree_item->session.session_id());
    }
}

void RouterManagerWindow::disconnectAllHosts()
{
    if (!router_proxy_)
        return;

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            tr("Are you sure you want to disconnect all hosts?"),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::No)
        return;

    beforeRequest();

    QTreeWidget* tree_hosts = ui->tree_hosts;
    for (int i = 0; i < tree_hosts->topLevelItemCount(); ++i)
    {
        HostTreeItem* tree_item = static_cast<HostTreeItem*>(tree_hosts->topLevelItem(i));
        if (tree_item)
            router_proxy_->stopSession(tree_item->session.session_id());
    }
}

void RouterManagerWindow::refreshUserList()
{
    if (router_proxy_)
    {
        beforeRequest();
        router_proxy_->refreshUserList();
    }
}

void RouterManagerWindow::addUser()
{
    QTreeWidget* tree_users = ui->tree_users;
    std::vector<std::u16string> users;

    for (int i = 0; i < tree_users->topLevelItemCount(); ++i)
        users.emplace_back(static_cast<UserTreeItem*>(tree_users->topLevelItem(i))->user.name);

    RouterUserDialog dialog(base::User(), users, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (router_proxy_)
        {
            beforeRequest();
            router_proxy_->addUser(dialog.user().serialize());
        }
    }
}

void RouterManagerWindow::modifyUser()
{
    QTreeWidget* tree_users = ui->tree_users;
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(tree_users->currentItem());
    if (!tree_item)
        return;

    std::vector<std::u16string> users;

    for (int i = 0; i < tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* current_item = static_cast<UserTreeItem*>(tree_users->topLevelItem(i));
        if (current_item->text(0).compare(tree_item->text(0), Qt::CaseInsensitive) != 0)
            users.emplace_back(current_item->user.name);
    }

    RouterUserDialog dialog(tree_item->user, users, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (router_proxy_)
        {
            beforeRequest();
            router_proxy_->modifyUser(dialog.user().serialize());
        }
    }
}

void RouterManagerWindow::deleteUser()
{
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui->tree_users->currentItem());
    if (!tree_item)
        return;

    int64_t entry_id = tree_item->user.entry_id;
    if (entry_id == 1)
    {
        QMessageBox::warning(this, tr("Warning"), tr("You cannot delete a built-in user."));
        return;
    }

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            tr("Are you sure you want to delete user \"%1\"?")
                                .arg(tree_item->text(0)),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
    {
        if (router_proxy_)
        {
            beforeRequest();
            router_proxy_->deleteUser(entry_id);
        }
    }
}

void RouterManagerWindow::onCurrentUserChanged(
    QTreeWidgetItem* /* current */, QTreeWidgetItem* /* previous */)
{
    ui->button_modify_user->setEnabled(true);
    ui->button_delete_user->setEnabled(true);
}

void RouterManagerWindow::onCurrentHostChanged(QTreeWidgetItem* current,
                                               QTreeWidgetItem* /* previous */)
{
    QTreeWidget* tree_host_info = ui->tree_host_info;
    tree_host_info->clear();

    HostTreeItem* host_item = reinterpret_cast<HostTreeItem*>(current);
    if (!host_item)
    {
        tree_host_info->setEnabled(false);
    }
    else
    {
        tree_host_info->setEnabled(true);

        const proto::Session& session = host_item->session;

        auto add_item = [=](const QString& name, const QString& value)
        {
            QTreeWidgetItem* item = new QTreeWidgetItem();
            item->setText(0, name);
            item->setText(1, value);

            tree_host_info->addTopLevelItem(item);
        };

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            static_cast<uint>(session.timepoint())), QLocale::ShortFormat);

        add_item(tr("Computer Name"), QString::fromStdString(session.computer_name()));
        add_item(tr("IP Address"), QString::fromStdString(session.ip_address()));
        add_item(tr("Connect Time"), time);
        const proto::Version& version = session.version();
        add_item(tr("Version"), QString("%1.%2.%3.%4")
            .arg(version.major()).arg(version.minor()).arg(version.patch()).arg(version.revision()));
        add_item(tr("Operating System"), QString::fromStdString(session.os_name()));

        proto::HostSessionData session_data;
        if (session_data.ParseFromString(session.session_data()))
        {
            QTreeWidgetItem* title_item = new QTreeWidgetItem();
            title_item->setText(0, tr("Host IDs"));

            tree_host_info->addTopLevelItem(title_item);

            for (int i = 0; i < session_data.host_id_size(); ++i)
            {
                QTreeWidgetItem* item = new QTreeWidgetItem();
                item->setText(0, tr("ID #%1").arg(i + 1));
                item->setText(1, QString::number(session_data.host_id(i)));

                title_item->addChild(item);
            }

            title_item->setExpanded(true);
        }

        for (int i = 0; i < tree_host_info->columnCount(); ++i)
            tree_host_info->resizeColumnToContents(i);
    }
}

void RouterManagerWindow::beforeRequest()
{
    ui->tab->setEnabled(false);
}

void RouterManagerWindow::afterRequest()
{
    ui->tab->setEnabled(true);
}

void RouterManagerWindow::saveHostsToFile()
{
    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(this,
                                                     tr("Save File"),
                                                     QString(),
                                                     tr("JSON files (*.json)"),
                                                     &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
        return;

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Could not open file for writing."),
                             QMessageBox::Ok);
        return;
    }

    QTreeWidget* tree = ui->tree_hosts;
    QJsonArray root_array;

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        const proto::Session& session =
            static_cast<HostTreeItem*>(tree->topLevelItem(i))->session;

        QJsonObject host_object;

        host_object.insert("session_id", QString::number(session.session_id()));
        host_object.insert("computer_name", QString::fromStdString(session.computer_name()));
        host_object.insert("operating_system", QString::fromStdString(session.os_name()));
        host_object.insert("ip_address", QString::fromStdString(session.ip_address()));

        QString version = QString("%1.%2.%3.%4")
            .arg(session.version().major()).arg(session.version().minor())
            .arg(session.version().patch()).arg(session.version().revision());
        host_object.insert("version", version);

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            static_cast<uint>(session.timepoint())), QLocale::ShortFormat);
        host_object.insert("connect_time", time);

        QJsonArray id_array;

        proto::HostSessionData session_data;
        if (session_data.ParseFromString(session.session_data()))
        {
            for (int i = 0; i < session_data.host_id_size(); ++i)
                id_array.append(QString::number(session_data.host_id(i)));
        }

        host_object.insert("host_ids", id_array);
        root_array.append(host_object);
    }

    QJsonObject root_object;
    root_object.insert("hosts", root_array);

    int64_t written = file.write(QJsonDocument(root_object).toJson());
    if (written <= 0)
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Unable to write file."),
                             QMessageBox::Ok);
        return;
    }
}

void RouterManagerWindow::saveRelaysToFile()
{
    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(this,
                                                     tr("Save File"),
                                                     QString(),
                                                     tr("JSON files (*.json)"),
                                                     &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
        return;

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Could not open file for writing."),
                             QMessageBox::Ok);
        return;
    }

    QTreeWidget* tree = ui->tree_relay;
    QJsonArray root_array;

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        const proto::Session& session =
            static_cast<RelayTreeItem*>(tree->topLevelItem(i))->session;

        QJsonObject relay_object;

        relay_object.insert("session_id", QString::number(session.session_id()));
        relay_object.insert("computer_name", QString::fromStdString(session.computer_name()));
        relay_object.insert("operating_system", QString::fromStdString(session.os_name()));
        relay_object.insert("ip_address", QString::fromStdString(session.ip_address()));

        QString version = QString("%1.%2.%3.%4")
            .arg(session.version().major()).arg(session.version().minor())
            .arg(session.version().patch()).arg(session.version().revision());
        relay_object.insert("version", version);

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            static_cast<uint>(session.timepoint())), QLocale::ShortFormat);
        relay_object.insert("connect_time", time);

        proto::RelaySessionData session_data;
        if (session_data.ParseFromString(session.session_data()))
            relay_object.insert("pool_size", QString::number(session_data.pool_size()));

        root_array.append(relay_object);
    }

    QJsonObject root_object;
    root_object.insert("relays", root_array);

    int64_t written = file.write(QJsonDocument(root_object).toJson());
    if (written <= 0)
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Unable to write file."),
                             QMessageBox::Ok);
        return;
    }
}

} // namespace client
