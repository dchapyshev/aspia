//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/manager/main_window.h"

#include "base/logging.h"
#include "peer/user.h"
#include "qt_base/application.h"
#include "router/manager/user_dialog.h"
#include "router/manager/router.h"
#include "router/manager/router_proxy.h"
#include "router/manager/router_window_proxy.h"
#include "router/manager/status_dialog.h"

#include <QMessageBox>
#include <QTimer>

namespace router {

namespace {

class PeerTreeItem : public QTreeWidgetItem
{
public:
    explicit PeerTreeItem(const proto::Peer& peer)
    {
        setText(0, QString::number(peer.peer_id()));
        setText(1, QString::fromStdString(peer.user_name()));
        setText(2, QString::fromStdString(peer.ip_address()));
    }

    ~PeerTreeItem() = default;

    uint64_t peer_id;

private:
    DISALLOW_COPY_AND_ASSIGN(PeerTreeItem);
};

class ProxyTreeItem : public QTreeWidgetItem
{
public:
    explicit ProxyTreeItem(const proto::Proxy& proxy)
    {
        setText(0, QString::fromStdString(proxy.address()));
        setText(1, QString::number(proxy.pool_size()));
    }

private:
    DISALLOW_COPY_AND_ASSIGN(ProxyTreeItem);
};

class UserTreeItem : public QTreeWidgetItem
{
public:
    explicit UserTreeItem(const proto::User& user)
        : user(user)
    {
        setText(0, QString::fromStdString(user.name()));

        if (user.flags() & peer::User::ENABLED)
            setIcon(0, QIcon(QLatin1String(":/img/user.png")));
        else
            setIcon(0, QIcon(QLatin1String(":/img/user-disabled.png")));
    }

    proto::User user;

private:
    DISALLOW_COPY_AND_ASSIGN(UserTreeItem);
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      status_dialog_(new StatusDialog(this)),
      window_proxy_(std::make_shared<RouterWindowProxy>(
        qt_base::Application::uiTaskRunner(), this))
{
    ui.setupUi(this);

    connect(ui.button_refresh_peers, &QPushButton::released, this, &MainWindow::refreshPeerList);
    connect(ui.button_disconnect_peer, &QPushButton::released, this, &MainWindow::disconnectPeer);
    connect(ui.button_refresh_proxy, &QPushButton::released, this, &MainWindow::refreshProxyList);
    connect(ui.button_refresh_users, &QPushButton::released, this, &MainWindow::refreshUserList);
    connect(ui.button_add_user, &QPushButton::released, this, &MainWindow::addUser);
    connect(ui.button_modify_user, &QPushButton::released, this, &MainWindow::modifyUser);
    connect(ui.button_delete_user, &QPushButton::released, this, &MainWindow::deleteUser);
    connect(ui.tree_users, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentUserChanged);
}

MainWindow::~MainWindow()
{
    window_proxy_->dettach();
}

void MainWindow::connectToRouter(const QString& address,
                                 uint16_t port,
                                 const QByteArray& public_key,
                                 const QString& user_name,
                                 const QString& password)
{
    peer_address_ = address;
    peer_port_ = port;

    status_dialog_->setStatus(tr("Connecting to %1:%2...").arg(address).arg(port));
    status_dialog_->show();
    status_dialog_->activateWindow();

    connect(status_dialog_, &StatusDialog::canceled, [this]()
    {
        router_proxy_.reset();
        close();
    });

    std::unique_ptr<Router> router = std::make_unique<Router>(
        window_proxy_, qt_base::Application::ioTaskRunner());

    router->setPublicKey(base::fromHex(public_key.toStdString()));
    router->setUserName(user_name.toStdU16String());
    router->setPassword(password.toStdU16String());

    router_proxy_ = std::make_unique<RouterProxy>(
        qt_base::Application::ioTaskRunner(), std::move(router));

    router_proxy_->connectToRouter(address.toStdU16String(), port);
}

void MainWindow::onConnected(const base::Version& peer_version)
{
    status_dialog_->hide();

    ui.statusbar->showMessage(tr("Connected to: %1:%2 (version %3)")
                              .arg(peer_address_)
                              .arg(peer_port_)
                              .arg(QString::fromStdString(peer_version.toString())));
    show();
    activateWindow();

    if (router_proxy_)
    {
        router_proxy_->refreshPeerList();
        router_proxy_->refreshProxyList();
        router_proxy_->refreshUserList();
    }
}

void MainWindow::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
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

void MainWindow::onAccessDenied(peer::ClientAuthenticator::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case peer::ClientAuthenticator::ErrorCode::SUCCESS:
            message = QT_TR_NOOP("Authentication successfully completed.");
            break;

        case peer::ClientAuthenticator::ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("Network authentication error.");
            break;

        case peer::ClientAuthenticator::ErrorCode::PROTOCOL_ERROR:
            message = QT_TR_NOOP("Violation of the data exchange protocol.");
            break;

        case peer::ClientAuthenticator::ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("An error occured while authenticating: wrong user name or password.");
            break;

        case peer::ClientAuthenticator::ErrorCode::SESSION_DENIED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        default:
            message = QT_TR_NOOP("An unknown error occurred.");
            break;
    }

    status_dialog_->setStatus(tr("Error: %1").arg(tr(message)));
    status_dialog_->show();
}

void MainWindow::onPeerList(std::shared_ptr<proto::PeerList> peer_list)
{
    ui.tree_peer->clear();

    for (int i = 0; i < peer_list->peer_size(); ++i)
        ui.tree_peer->addTopLevelItem(new PeerTreeItem(peer_list->peer(i)));

    afterRequest();
}

void MainWindow::onPeerResult(std::shared_ptr<proto::PeerResult> peer_result)
{
    refreshPeerList();
    afterRequest();
}

void MainWindow::onProxyList(std::shared_ptr<proto::ProxyList> proxy_list)
{
    ui.tree_proxy->clear();

    for (int i = 0; i < proxy_list->proxy_size(); ++i)
        ui.tree_proxy->addTopLevelItem(new ProxyTreeItem(proxy_list->proxy(i)));

    afterRequest();
}

void MainWindow::onUserList(std::shared_ptr<proto::UserList> user_list)
{
    ui.tree_users->clear();

    for (int i = 0; i < user_list->user_size(); ++i)
        ui.tree_users->addTopLevelItem(new UserTreeItem(user_list->user(i)));

    afterRequest();
}

void MainWindow::onUserResult(std::shared_ptr<proto::UserResult> user_result)
{
    refreshUserList();
    afterRequest();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (router_proxy_)
        router_proxy_->disconnectFromRouter();

    QApplication::quit();
}

void MainWindow::refreshPeerList()
{
    if (router_proxy_)
    {
        beforeRequest();
        router_proxy_->refreshPeerList();
    }
}

void MainWindow::disconnectPeer()
{
    PeerTreeItem* tree_item = static_cast<PeerTreeItem*>(ui.tree_peer->currentItem());
    if (!tree_item)
        return;

    if (router_proxy_)
    {
        beforeRequest();
        router_proxy_->disconnectPeer(tree_item->peer_id);
    }
}

void MainWindow::refreshProxyList()
{
    if (router_proxy_)
    {
        beforeRequest();
        router_proxy_->refreshProxyList();
    }
}

void MainWindow::refreshUserList()
{
    if (router_proxy_)
    {
        beforeRequest();
        router_proxy_->refreshUserList();
    }
}

void MainWindow::addUser()
{
    QVector<proto::User> users;

    for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
        users.append(static_cast<UserTreeItem*>(ui.tree_users->topLevelItem(i))->user);

    UserDialog dialog(users, -1, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (router_proxy_)
        {
            beforeRequest();
            router_proxy_->addUser(dialog.user());
        }
    }
}

void MainWindow::modifyUser()
{
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!tree_item)
        return;

    QVector<proto::User> users;
    int user_index = -1;

    for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* current_item = static_cast<UserTreeItem*>(ui.tree_users->topLevelItem(i));
        if (current_item->text(0).compare(tree_item->text(0), Qt::CaseInsensitive) == 0)
            user_index = i;

        users.append(current_item->user);
    }

    UserDialog dialog(users, user_index, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (router_proxy_)
        {
            beforeRequest();
            router_proxy_->modifyUser(dialog.user());
        }
    }
}

void MainWindow::deleteUser()
{
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!tree_item)
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to delete user \"%1\"?")
                              .arg(tree_item->text(0)),
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        if (router_proxy_)
        {
            beforeRequest();
            router_proxy_->deleteUser(tree_item->user.entry_id());
        }
    }
}

void MainWindow::onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    ui.button_modify_user->setEnabled(true);
    ui.button_delete_user->setEnabled(true);
}

void MainWindow::beforeRequest()
{
    ui.tab->setEnabled(false);
}

void MainWindow::afterRequest()
{
    ui.tab->setEnabled(true);
}

} // namespace router
