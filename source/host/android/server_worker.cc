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

#include "host/android/server_worker.h"

#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>
#include <QVariant>

#include <optional>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "base/peer/host_id.h"
#include "base/peer/user_list.h"
#include "build/build_config.h"
#include "host/client.h"
#include "host/database.h"
#include "host/host_user_list.h"
#include "host/router_manager.h"
#include "host/user_settings.h"
#include "host/android/desktop_agent.h"
#include "host/android/desktop_client.h"
#include "host/android/file_client.h"
#include "proto/peer.h"
#include "proto/user.h"

namespace {

const char kScreenMonitorClass[] = "org/aspia/host/ScreenMonitor";

// A host serves a single user, so only a few simultaneous handshakes and a low per-address rate are
// expected. Tight caps limit the damage of a flood; latecomers retry shortly.
constexpr int kMaxPendingConnections = 10;
constexpr int kMaxConnectionsPerMinute = 30;

// Per-connection socket buffers.
constexpr int kReadBufferSize = 2 * 1024 * 1024; // 2 MB.
constexpr int kWriteBufferSize = 2 * 1024 * 1024; // 2 MB.

// Guards the single ServerWorker instance against the screen-state JNI callback, delivered on the Android
// main thread. At most one server exists, so a bare pointer is enough.
QMutex g_mutex;
ServerWorker* g_instance = nullptr;

//--------------------------------------------------------------------------------------------------
// Called by ScreenMonitor when the display turns on or off. Hops to the server's I/O thread.
void screenInteractiveChanged(JNIEnv* /* env */, jclass /* clazz */, jboolean interactive)
{
    QMutexLocker locker(&g_mutex);
    if (g_instance)
    {
        QMetaObject::invokeMethod(g_instance, "onScreenInteractiveChanged", Qt::QueuedConnection,
                                  Q_ARG(bool, interactive == JNI_TRUE));
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ServerWorker::ServerWorker()
    : Worker(Thread::AsioDispatcher)
{
    LOG(INFO) << "Ctor";
    qRegisterMetaType<QList<ClientInfo>>();

    static bool registered = false;
    if (!registered)
    {
        const JNINativeMethod methods[] =
        {
            { "nativeOnInteractiveChanged", "(Z)V", reinterpret_cast<void*>(screenInteractiveChanged) }
        };

        QJniEnvironment env;
        if (!env.registerNativeMethods(kScreenMonitorClass, methods, 1))
            LOG(ERROR) << "Unable to register native methods for ScreenMonitor";
        else
            registered = true;
    }

    QMutexLocker locker(&g_mutex);
    g_instance = this;
}

//--------------------------------------------------------------------------------------------------
ServerWorker::~ServerWorker()
{
    LOG(INFO) << "Dtor";

    QMutexLocker locker(&g_mutex);
    if (g_instance == this)
        g_instance = nullptr;
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onStart()
{
    if (tcp_server_)
    {
        LOG(ERROR) << "Server is already started";
        return;
    }

    Database& db = Database::instance();

    // Created here (on the I/O thread) so its asio acceptor binds to this thread's io_context.
    tcp_server_ = new TcpServer(this);
    connect(tcp_server_, &TcpServer::sig_newConnection, this, &ServerWorker::onNewConnection);

    tcp_server_->setMaxPendingConnections(kMaxPendingConnections);
    tcp_server_->setMaxConnectionsPerMinute(kMaxConnectionsPerMinute);
    tcp_server_->start(db.tcpPort());
    tcp_server_->setUserList(SharedPointer<UserList>(new HostUserList));

    LOG(INFO) << "Host server started on port" << db.tcpPort();

    // Shared engine for all desktop clients; it owns them and captures only while at least one is
    // connected.
    desktop_agent_ = new DesktopAgent(this);

    // The router connection is bound to the application lifecycle: dropped while backgrounded or with
    // the screen off, restored on return. The app-state signal comes from the GUI thread and is
    // delivered here queued; screen on/off arrives through ScreenMonitor's JNI callback, which also
    // reports the current state once started. The state may have changed before the subscriptions were
    // in place, so seed the flag with the live value instead of assuming an active start.
    app_active_ = (qGuiApp->applicationState() == Qt::ApplicationActive);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged,
            this, &ServerWorker::onApplicationStateChanged, Qt::QueuedConnection);

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (context.isValid())
        {
            QJniObject::callStaticMethod<void>(
                kScreenMonitorClass, "start", "(Landroid/content/Context;)V", context.object());
        }
        return QVariant();
    });

    updateRouterConnection();
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onStop()
{
    if (!tcp_server_)
        return;

    desktop_agent_.reset();
    router_manager_.reset();
    tcp_server_.reset();

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (context.isValid())
        {
            QJniObject::callStaticMethod<void>(
                kScreenMonitorClass, "stop", "(Landroid/content/Context;)V", context.object());
        }
        return QVariant();
    });

    LOG(INFO) << "Host server stopped";
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onNewConnection()
{
    while (tcp_server_ && tcp_server_->hasReadyConnections())
    {
        TcpChannel* tcp_channel = tcp_server_->nextReadyConnection();
        if (!tcp_channel)
            break;

        startClient(tcp_channel);
    }
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onNewRelayConnection()
{
    if (!router_manager_)
        return;

    while (router_manager_->hasReadyConnections())
    {
        std::optional<RouterManager::ReadyConnection> connection =
            router_manager_->nextReadyConnection();
        if (!connection.has_value())
            continue;

        startClient(connection->tcp_channel, connection->stun_host, connection->stun_port);
    }
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onClientStarted()
{
    Client* client = dynamic_cast<Client*>(sender());
    if (!client)
        return;

    LOG(INFO) << "Client started:" << client->clientId();

    ClientInfo info;
    info.client_id = client->clientId();
    info.computer_name = client->computerName();
    info.display_name = client->displayName();
    info.session_type = client->sessionType();

    connected_clients_.append(info);
    emit sig_connectedClientsChanged(connected_clients_);
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    if (!client)
        return;

    LOG(INFO) << "Client finished:" << client->clientId();

    const quint32 client_id = client->clientId();
    connected_clients_.removeIf([client_id](const ClientInfo& info) { return info.client_id == client_id; });

    // Only file clients are owned here; desktop clients are owned by the desktop agent.
    if (file_clients_.removeOne(client))
        client->deleteLater();

    emit sig_connectedClientsChanged(connected_clients_);

    // A session may have been the only reason the router was kept online while in the background; if it
    // was the last one, drop the connection.
    updateRouterConnection();
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onRouterStateChanged(const proto::user::RouterState& state)
{
    QString router;

    if (state.state() != proto::user::RouterState::DISABLED)
    {
        Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(QString::fromStdString(state.host_name()));
        address.setPort(static_cast<quint16>(state.host_port()));
        router = address.toString();
    }

    emit sig_routerStateChanged(static_cast<int>(state.state()), router);
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onCredentialsChanged(HostId host_id, const SecureString& password)
{
    emit sig_credentialsChanged(hostIdToString(host_id), password.toString());
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onApplicationStateChanged(Qt::ApplicationState state)
{
    app_active_ = (state == Qt::ApplicationActive);
    updateRouterConnection();
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::onScreenInteractiveChanged(bool interactive)
{
    // Locking the screen does not change the Qt application state, so this is a separate signal.
    screen_on_ = interactive;
    updateRouterConnection();
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::connectToRouter()
{
    if (router_manager_)
        return;

    router_manager_ = new RouterManager(this);

    connect(router_manager_, &RouterManager::sig_clientConnected,
            this, &ServerWorker::onNewRelayConnection);
    connect(router_manager_, &RouterManager::sig_routerStateChanged,
            this, &ServerWorker::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged,
            this, &ServerWorker::onCredentialsChanged);

    router_manager_->start();

    // The desktop host pushes the allowed one-time session types from the user session; here they are
    // taken directly from the settings.
    router_manager_->onOneTimeSessionsChanged(UserSettings().oneTimeSessions());
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::disconnectFromRouter()
{
    if (!router_manager_)
        return;

    // reset() schedules deleteLater() but does not disconnect; sever the connections first so nothing
    // (e.g. a reconnect timer tick before the deletion) re-emits a state on top of DISABLED below.
    router_manager_->disconnect(this);
    router_manager_.reset();

    // Reflect the offline state on the connection screen: the assigned ID and one-time password are no
    // longer valid, and a fresh password is issued on the next connect.
    emit sig_routerStateChanged(static_cast<int>(proto::user::RouterState::DISABLED), QString());
    emit sig_credentialsChanged(QString(), QString());

    LOG(INFO) << "Router connection closed (host went to background)";
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::updateRouterConnection()
{
    // The window may already be gone (server stopped); nothing to manage then.
    if (!tcp_server_)
        return;

    // The host is reachable through the router only while the user is looking at the app (foreground
    // with the screen on). An active session keeps the connection regardless: it holds the app alive
    // through its foreground service, and its relay leg must not be torn down. Otherwise Android would
    // freeze the process in the background anyway and the router would drop the host by timeout, so the
    // connection is closed now for a clean, immediate offline state.
    const bool should_be_online = (app_active_ && screen_on_) || !connected_clients_.isEmpty();

    if (should_be_online)
    {
        if (Database::instance().isRouterEnabled())
            connectToRouter();
    }
    else
    {
        disconnectFromRouter();
    }
}

//--------------------------------------------------------------------------------------------------
void ServerWorker::startClient(TcpChannel* tcp_channel, const QString& stun_host, quint16 stun_port)
{
    tcp_channel->setParent(this);
    tcp_channel->setReadBufferSize(kReadBufferSize);
    tcp_channel->setWriteBufferSize(kWriteBufferSize);

    const auto session_type = static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());

    // Desktop clients are owned and tracked by the shared agent; file transfer clients are kept here.
    if (session_type == proto::peer::SESSION_TYPE_DESKTOP)
    {
        DesktopClient* desktop_client = new DesktopClient(tcp_channel, this);
        desktop_client->setFeature(Client::FEATURE_UDP, true);
        desktop_client->setFeature(Client::FEATURE_BANDWIDTH, true);

        // Track the client for the connected-clients list; the agent owns and deletes it.
        connect(desktop_client, &Client::sig_started, this, &ServerWorker::onClientStarted);
        connect(desktop_client, &Client::sig_finished, this, &ServerWorker::onClientFinished);

        desktop_agent_->addClient(desktop_client);
        desktop_client->start(stun_host, stun_port);
        return;
    }

    if (session_type != proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        LOG(ERROR) << "Unsupported session type:" << session_type;
        tcp_channel->deleteLater();
        return;
    }

    FileClient* file_client = new FileClient(tcp_channel, this);
    file_client->setFeature(Client::FEATURE_UDP, true);

    connect(file_client, &Client::sig_started, this, &ServerWorker::onClientStarted);
    connect(file_client, &Client::sig_finished, this, &ServerWorker::onClientFinished);

    file_clients_.append(file_client);

    // Direct connections carry no STUN endpoint; relayed connections get one from the router.
    file_client->start(stun_host, stun_port);
}
