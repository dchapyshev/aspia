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

#include "client/android/main_window.h"

#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QJniObject>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <optional>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/application.h"
#include "client/host_url.h"
#include "client/router.h"
#include "client/android/authorization_dialog.h"
#include "client/android/chat_window.h"
#include "client/android/desktop_window.h"
#include "client/android/file_transfer_window.h"
#include "client/android/local_widget.h"
#include "client/android/master_password_dialog.h"
#include "client/android/remote_widget.h"
#include "client/android/routers_widget.h"
#include "client/android/settings_widget.h"
#include "client/config.h"
#include "client/database.h"
#include "client/master_password.h"
#include "common/android/app_bar.h"
#include "common/android/bottom_navigation_bar.h"
#include "common/android/message_dialog.h"
#include "proto/peer.h"

namespace {

// android.view.WindowManager.LayoutParams display cutout modes.
constexpr int kCutoutModeDefault = 0;
constexpr int kCutoutModeShortEdges = 1;

// android.content.Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY.
constexpr int kFlagActivityLaunchedFromHistory = 0x00100000;

// Re-lock the app once it has spent at least this long in the background.
constexpr int kLockTimeoutSec = 3 * 60;

// Order of the pages in the stacked content and of the bottom navigation items.
enum Section
{
    SECTION_LOCAL = 0,
    SECTION_REMOTE,
    SECTION_ROUTERS,
    SECTION_SETTINGS
};

//--------------------------------------------------------------------------------------------------
// Allows or forbids the window from drawing into the display cutout area (the camera notch). Pairs
// with clearing WA_ContentsMarginsRespectsSafeArea so the content actually fills the cutout instead
// of being letterboxed below it.
void setDrawIntoCutout(bool enable)
{
    const int mode = enable ? kCutoutModeShortEdges : kCutoutModeDefault;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([mode]() -> QVariant
    {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid())
            return QVariant();

        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid())
            return QVariant();

        QJniObject params = window.callObjectMethod(
            "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
        if (!params.isValid())
            return QVariant();

        params.setField<jint>("layoutInDisplayCutoutMode", mode);
        window.callMethod<void>("setAttributes", "(Landroid/view/WindowManager$LayoutParams;)V",
                                params.object());
        return QVariant();
    });
}

//--------------------------------------------------------------------------------------------------
// Returns the aspia:// link the activity was launched with, if any. A relaunch from the recents
// screen re-delivers the original intent - it must not restart the connection.
QString startUrlFromIntent()
{
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid())
        return QString();

    QJniObject intent = activity.callObjectMethod("getIntent", "()Landroid/content/Intent;");
    if (!intent.isValid())
        return QString();

    if (intent.callMethod<jint>("getFlags") & kFlagActivityLaunchedFromHistory)
        return QString();

    QJniObject data = intent.callObjectMethod("getDataString", "()Ljava/lang/String;");
    if (!data.isValid())
        return QString();

    QString url = data.toString();
    return HostUrl::isHostUrl(url) ? url : QString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::AndroidMainWindow(QWidget* parent)
    : QWidget(parent),
      app_bar_(new AppBar(this)),
      content_(new QStackedWidget(this)),
      navigation_(new BottomNavigationBar(this))
{
    LocalWidget* local = new LocalWidget(this);
    RoutersWidget* routers = new RoutersWidget(this);
    RemoteWidget* remote = new RemoteWidget(this);
    SettingsWidget* settings = new SettingsWidget(this);

    content_->addWidget(local);
    content_->addWidget(remote);
    content_->addWidget(routers);
    content_->addWidget(settings);

    connect(routers, &RoutersWidget::appBarActionsChanged,
            this, &AndroidMainWindow::onRouterActionsChanged);
    connect(routers, &RoutersWidget::sig_titleChanged,
            this, &AndroidMainWindow::onRoutersTitleChanged);
    connect(local, &LocalWidget::sig_titleChanged,
            this, &AndroidMainWindow::onLocalTitleChanged);
    connect(local, &LocalWidget::sig_appBarActionsChanged,
            this, &AndroidMainWindow::onLocalActionsChanged);
    connect(remote, &RemoteWidget::sig_titleChanged,
            this, &AndroidMainWindow::onRemoteTitleChanged);
    connect(local, &LocalWidget::sig_searchModeChanged,
            this, &AndroidMainWindow::onSearchModeChanged);
    connect(remote, &RemoteWidget::sig_searchModeChanged,
            this, &AndroidMainWindow::onSearchModeChanged);
    connect(settings, &SettingsWidget::sig_titleChanged,
            this, &AndroidMainWindow::onSettingsTitleChanged);
    connect(settings, &SettingsWidget::sig_appBarActionsChanged,
            this, &AndroidMainWindow::onSettingsActionsChanged);
    connect(local, &LocalWidget::sig_connectHost, this, &AndroidMainWindow::onConnectHost);
    connect(remote, &RemoteWidget::sig_connectHost, this, &AndroidMainWindow::onConnectRouterHost);
    connect(app_bar_, &AppBar::sig_backClicked, this, &AndroidMainWindow::onBackClicked);
    connect(app_bar_, &AppBar::sig_searchTextChanged, this, &AndroidMainWindow::onSearchTextChanged);

    navigation_->addItem(tr("Local"), ":/img/folder.svg");
    navigation_->addItem(tr("Remote"), ":/img/workspace.svg");
    navigation_->addItem(tr("Routers"), ":/img/stack.svg");
    navigation_->addItem(tr("Settings"), ":/img/settings.svg");

    // The address book shell. While a desktop connection is active it is replaced by the desktop view.
    shell_ = new QWidget(this);
    QVBoxLayout* shell_layout = new QVBoxLayout(shell_);
    shell_layout->setContentsMargins(0, 0, 0, 0);
    shell_layout->setSpacing(0);
    shell_layout->addWidget(app_bar_);
    shell_layout->addWidget(content_, 1);
    shell_layout->addWidget(navigation_);

    root_stack_ = new QStackedWidget(this);
    root_stack_->addWidget(shell_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(root_stack_);

    connect(navigation_, &BottomNavigationBar::sig_currentChanged,
            this, &AndroidMainWindow::onSectionChanged);

    onSectionChanged(navigation_->currentIndex());

    connect(GuiApplication::instance(), &QGuiApplication::applicationStateChanged,
            this, &AndroidMainWindow::onApplicationStateChanged);
    connect(Application::instance(), &Application::sig_urlOpened,
            this, &AndroidMainWindow::onUrlOpened);

    // The gate runs once the event loop is active so the window is laid out and the dialog scrim
    // covers it.
    QMetaObject::invokeMethod(this, [this]() { runMasterPasswordGate(); }, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::~AndroidMainWindow() = default;

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSectionChanged(int index)
{
    content_->setCurrentIndex(index);

    // The back button belongs to the remote host view; any tab switch clears it.
    app_bar_->setBackVisible(false);

    RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS));
    if (index != SECTION_ROUTERS && routers)
        routers->resetToList();

    LocalWidget* local = qobject_cast<LocalWidget*>(content_->widget(SECTION_LOCAL));
    RemoteWidget* remote = qobject_cast<RemoteWidget*>(content_->widget(SECTION_REMOTE));
    SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS));

    if (index != SECTION_SETTINGS && settings)
        settings->resetToSettings();

    QList<QWidget*> actions;
    if (index == SECTION_LOCAL && local)
        actions = local->appBarActions();
    else if (index == SECTION_ROUTERS && routers)
        actions = routers->appBarActions();
    else if (index == SECTION_REMOTE && remote)
        actions = remote->appBarActions();
    else if (index == SECTION_SETTINGS && settings)
        actions = settings->appBarActions();
    app_bar_->setActions(actions);

    // Refresh from storage when a browsing screen becomes visible (picks up changes made elsewhere).
    if (index == SECTION_LOCAL && local)
        local->reload();
    else if (index == SECTION_REMOTE && remote)
        remote->reload();

    switch (index)
    {
        case SECTION_LOCAL:
            app_bar_->setTitle(tr("Local"));
            break;

        case SECTION_REMOTE:
            app_bar_->setTitle(tr("Remote"));
            break;

        case SECTION_ROUTERS:
            app_bar_->setTitle(tr("Routers"));
            break;

        case SECTION_SETTINGS:
            app_bar_->setTitle(tr("Settings"));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRouterActionsChanged()
{
    RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS));
    if (navigation_->currentIndex() == SECTION_ROUTERS && routers)
        app_bar_->setActions(routers->appBarActions());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onLocalActionsChanged()
{
    LocalWidget* local = qobject_cast<LocalWidget*>(content_->widget(SECTION_LOCAL));
    if (navigation_->currentIndex() == SECTION_LOCAL && local)
        app_bar_->setActions(local->appBarActions());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSearchModeChanged(bool active)
{
    if (active)
    {
        // The field text is routed to the active tab by onSearchTextChanged().
        app_bar_->setActions({});
        app_bar_->setBackVisible(true);
        app_bar_->setSearchMode(true);
    }
    else
    {
        app_bar_->setSearchMode(false);
        // Restore the bar to the current tab's default state.
        onSectionChanged(navigation_->currentIndex());
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSearchTextChanged(const QString& text)
{
    switch (navigation_->currentIndex())
    {
        case SECTION_LOCAL:
            if (LocalWidget* local = qobject_cast<LocalWidget*>(content_->widget(SECTION_LOCAL)))
                local->searchQuery(text);
            break;

        case SECTION_REMOTE:
            if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(content_->widget(SECTION_REMOTE)))
                remote->searchQuery(text);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onLocalTitleChanged(const QString& title, bool back_visible)
{
    if (navigation_->currentIndex() != SECTION_LOCAL)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Local") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRoutersTitleChanged(const QString& title, bool back_visible)
{
    if (navigation_->currentIndex() != SECTION_ROUTERS)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Routers") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRemoteTitleChanged(const QString& title, bool back_visible)
{
    if (navigation_->currentIndex() != SECTION_REMOTE)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Remote") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSettingsTitleChanged(const QString& title, bool back_visible)
{
    if (navigation_->currentIndex() != SECTION_SETTINGS)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Settings") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSettingsActionsChanged()
{
    SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS));
    if (navigation_->currentIndex() == SECTION_SETTINGS && settings)
        app_bar_->setActions(settings->appBarActions());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onBackClicked()
{
    switch (navigation_->currentIndex())
    {
        case SECTION_LOCAL:
            if (LocalWidget* local = qobject_cast<LocalWidget*>(content_->widget(SECTION_LOCAL)))
                local->goBack();
            break;

        case SECTION_REMOTE:
            if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(content_->widget(SECTION_REMOTE)))
                remote->goBack();
            break;

        case SECTION_ROUTERS:
            if (RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS)))
                routers->goBack();
            break;

        case SECTION_SETTINGS:
            if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS)))
                settings->goBack();
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onConnectHost(qint64 entry_id, proto::peer::SessionType session_type)
{
    std::optional<HostConfig> host = Database::instance().findHost(entry_id);
    if (!host.has_value())
        return;

    openSession(*host, session_type);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onConnectRouterHost(const HostConfig& host, proto::peer::SessionType session_type)
{
    openSession(host, session_type);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openSession(HostConfig host, proto::peer::SessionType session_type)
{
    // Only a single session is supported at a time.
    if (desktop_ || file_transfer_ || chat_)
        return;

    if (host.username().isEmpty() || host.password().isEmpty())
    {
        AuthorizationDialog dialog(host.routerId() > 0, this);
        dialog.setUserName(host.username());

        if (dialog.exec() != QDialog::Accepted)
            return;

        host.setUsername(dialog.userName());
        host.setPassword(dialog.password());
    }

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            openDesktop(host);
            break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            openFileTransfer(host);
            break;

        case proto::peer::SESSION_TYPE_CHAT:
            openChat(host);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openDesktop(const HostConfig& host)
{
    // Only a single desktop connection is supported at a time.
    if (desktop_)
        return;

    // The desktop view takes over the whole window until the connection is closed.
    desktop_ = new DesktopWindow(host);
    connect(desktop_, &DesktopWindow::sig_closed, this, &AndroidMainWindow::onDesktopClosed);

    root_stack_->addWidget(desktop_);
    root_stack_->setCurrentWidget(desktop_);

    // Hide the system bars and let the desktop view fill the whole screen, including the camera
    // cutout: allow drawing into the cutout and stop the layout from reserving the safe area.
    setDrawIntoCutout(true);
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    showFullScreen();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onDesktopClosed()
{
    if (!desktop_)
        return;

    root_stack_->setCurrentWidget(shell_);
    root_stack_->removeWidget(desktop_);
    desktop_->deleteLater();
    desktop_ = nullptr;

    setDrawIntoCutout(false);
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, true);
    showMaximized();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openFileTransfer(const HostConfig& host)
{
    // The file transfer screen is a regular page: the system bars stay visible (no full-screen or
    // cutout drawing, unlike the desktop view).
    file_transfer_ = new FileTransferWindow(host);
    connect(file_transfer_, &FileTransferWindow::sig_closed, this, &AndroidMainWindow::onFileTransferClosed);

    root_stack_->addWidget(file_transfer_);
    root_stack_->setCurrentWidget(file_transfer_);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onFileTransferClosed()
{
    if (!file_transfer_)
        return;

    root_stack_->setCurrentWidget(shell_);
    root_stack_->removeWidget(file_transfer_);
    file_transfer_->deleteLater();
    file_transfer_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openChat(const HostConfig& host)
{
    chat_ = new ChatWindow(host);
    connect(chat_, &ChatWindow::sig_closed, this, &AndroidMainWindow::onChatClosed);

    root_stack_->addWidget(chat_);
    root_stack_->setCurrentWidget(chat_);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onChatClosed()
{
    if (!chat_)
        return;

    root_stack_->setCurrentWidget(shell_);
    root_stack_->removeWidget(chat_);
    chat_->deleteLater();
    chat_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::connectToUrl(const QString& url)
{
    LOG(INFO) << "Connect to URL:" << url;

    HostUrl host_url = HostUrl::fromString(url);
    if (!host_url.isValid())
    {
        MessageDialog::info(this, tr("Connection by link"), tr("Invalid link."));
        return;
    }

    // Only a single session is supported at a time.
    if (desktop_ || file_transfer_ || chat_)
    {
        MessageDialog::info(this, tr("Connection by link"),
                            tr("Another session is active. Close it and open the link again."));
        return;
    }

    proto::peer::SessionType session_type = host_url.sessionType();
    if (session_type != proto::peer::SESSION_TYPE_DESKTOP &&
        session_type != proto::peer::SESSION_TYPE_FILE_TRANSFER &&
        session_type != proto::peer::SESSION_TYPE_CHAT)
    {
        MessageDialog::info(this, tr("Connection by link"),
                            tr("The session type from the link is not supported on this device."));
        return;
    }

    if (host_url.isRouterHost())
    {
        qint64 router_id = -1;
        QList<RouterConfig> routers = Database::instance().routerList();
        for (const RouterConfig& router_config : std::as_const(routers))
        {
            if (router_config.guid() == host_url.routerGuid())
            {
                router_id = router_config.routerId();
                break;
            }
        }

        if (router_id <= 0)
        {
            MessageDialog::info(this, tr("Connection by link"),
                tr("The router referenced by the link was not found in the address book."));
            return;
        }

        // The credentials of a router host live in its address book record on the router, so
        // fetch the record before connecting. If the router is not connected, connect right away
        // and let the authorization dialog ask for the credentials.
        Router* router = Router::instance(router_id);
        if (router && router->status() == Router::Status::ONLINE)
        {
            HostId host_id = host_url.hostId();

            router->searchHosts(hostIdToString(host_id), this,
                [this, router_id, host_id, session_type](const Router::HostList& list)
            {
                HostConfig host;
                host.setRouterId(router_id);
                host.setAddress(hostIdToString(host_id));

                for (const Router::Host& entry : std::as_const(list.hosts))
                {
                    if (entry.host_id != host_id)
                        continue;

                    host.setName(entry.display_name.isEmpty() ? entry.computer_name :
                                                                entry.display_name);
                    host.setUsername(entry.user_name);
                    host.setPassword(entry.password);
                    break;
                }

                openSession(host, session_type);
            });
            return;
        }

        HostConfig host;
        host.setRouterId(router_id);
        host.setAddress(hostIdToString(host_url.hostId()));
        openSession(host, session_type);
    }
    else
    {
        std::optional<HostConfig> host = Database::instance().findHost(host_url.entryId());
        if (!host.has_value())
        {
            MessageDialog::info(this, tr("Connection by link"),
                tr("The host referenced by the link was not found in the address book."));
            return;
        }

        openSession(*host, session_type);
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationActive)
    {
        // Back in the foreground: re-lock if the background timeout has elapsed.
        if (relocking_ || !MasterPassword::isSet() || !background_since_.isValid())
            return;

        const qint64 elapsed = background_since_.secsTo(QDateTime::currentDateTime());
        background_since_ = QDateTime();

        if (elapsed >= kLockTimeoutSec)
            relock();
    }
    else if (!relocking_ && !background_since_.isValid())
    {
        // Leaving the foreground: remember when, ignoring the transitions caused by the lock prompt.
        background_since_ = QDateTime::currentDateTime();
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onUrlOpened(const QString& url)
{
    if (!unlocked_)
    {
        pending_url_ = url;
        return;
    }

    connectToUrl(url);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::runMasterPasswordGate()
{
    const MasterPasswordDialog::Mode mode = MasterPassword::isSet() ?
        MasterPasswordDialog::Mode::UNLOCK : MasterPasswordDialog::Mode::CREATE;

    // The prompt itself sends the app through the background states (the fingerprint sheet), so guard
    // against the timer treating that as a real background trip.
    relocking_ = true;

    // The data cryptor is invalid until the dialog unlocks it, so quit if the user cancels.
    MasterPasswordDialog dialog(mode, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        QCoreApplication::quit();
        return;
    }

    relocking_ = false;
    onUnlocked();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onUnlocked()
{
    if (LocalWidget* local = qobject_cast<LocalWidget*>(content_->widget(SECTION_LOCAL)))
        local->reload();

    if (RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS)))
        routers->reload();

    if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(content_->widget(SECTION_REMOTE)))
        remote->reload();

    unlocked_ = true;

    // Open the link that started the application or arrived while the gate was on the screen.
    QString url = pending_url_.isEmpty() ? startUrlFromIntent() : pending_url_;
    pending_url_.clear();
    if (!url.isEmpty())
        connectToUrl(url);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::relock()
{
    relocking_ = true;

    // The data cryptor stays open; this only re-verifies the user (password or fingerprint).
    MasterPasswordDialog dialog(MasterPasswordDialog::Mode::UNLOCK, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        QCoreApplication::quit();
        return;
    }

    relocking_ = false;
}
