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
#include <QKeyEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWindow>

#include <optional>

#include "base/auto_qpointer.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/config.h"
#include "client/android/authorization_window.h"
#include "client/android/chat_window.h"
#include "client/android/desktop_window.h"
#include "client/android/file_transfer_window.h"
#include "client/android/local_widget.h"
#include "client/android/master_password_dialog.h"
#include "client/android/remote_widget.h"
#include "client/android/router_widget.h"
#include "client/android/settings_widget.h"
#include "client/android/two_factor_dialog.h"
#include "client/router/router_controller.h"
#include "client/storage/database_migration.h"
#include "client/storage/local_host_store.h"
#include "client/storage/master_password.h"
#include "client/storage/router_host_store.h"
#include "client/storage/router_store.h"
#include "client/workers/update_worker.h"
#include "common/android/app_bar.h"
#include "common/android/bottom_navigation_bar.h"
#include "common/android/bottom_sheet.h"
#include "common/android/keyboard_inset.h"
#include "common/android/menu.h"
#include "common/android/message_dialog.h"
#include "proto/peer.h"
#include "proto/router_constants.h"

namespace {

// android.view.WindowManager.LayoutParams display cutout modes.
constexpr int kCutoutModeDefault = 0;
constexpr int kCutoutModeShortEdges = 1;

// Re-lock the app once it has spent at least this long in the background.
constexpr Minutes kLockTimeout{ 3 };

// Order of the pages in the stacked content and of the bottom navigation items.
enum Section
{
    SECTION_LOCAL = 0,
    SECTION_REMOTE,
    SECTION_ROUTER,
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

} // namespace

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::AndroidMainWindow(QWidget* parent)
    : QWidget(parent),
      app_bar_(new AppBar(this)),
      stack_content_(new QStackedWidget(this)),
      nav_bar_(new BottomNavigationBar(this))
{
    RouterController* router_controller = new RouterController(this);
    connect(router_controller, &RouterController::sig_twoFactorRequired,
            this, &AndroidMainWindow::onTwoFactorRequired);

    LocalWidget* local = new LocalWidget(this);
    RouterWidget* router_widget = new RouterWidget(this);
    RemoteWidget* remote = new RemoteWidget(this);
    SettingsWidget* settings = new SettingsWidget(this);

    stack_content_->addWidget(local);
    stack_content_->addWidget(remote);
    stack_content_->addWidget(router_widget);
    stack_content_->addWidget(settings);

    connect(router_widget, &RouterWidget::sig_twoFactorClicked, this, &AndroidMainWindow::onTwoFactorRequired);
    connect(router_widget, &RouterWidget::sig_appBarActionsChanged, this, &AndroidMainWindow::onRouterActionsChanged);
    connect(router_widget, &RouterWidget::sig_titleChanged, this, &AndroidMainWindow::onRouterTitleChanged);
    connect(local, &LocalWidget::sig_titleChanged, this, &AndroidMainWindow::onLocalTitleChanged);
    connect(local, &LocalWidget::sig_appBarActionsChanged, this, &AndroidMainWindow::onLocalActionsChanged);
    connect(remote, &RemoteWidget::sig_titleChanged, this, &AndroidMainWindow::onRemoteTitleChanged);
    connect(local, &LocalWidget::sig_searchModeChanged, this, &AndroidMainWindow::onSearchModeChanged);
    connect(remote, &RemoteWidget::sig_searchModeChanged, this, &AndroidMainWindow::onSearchModeChanged);
    connect(settings, &SettingsWidget::sig_titleChanged, this, &AndroidMainWindow::onSettingsTitleChanged);
    connect(settings, &SettingsWidget::sig_appBarActionsChanged, this, &AndroidMainWindow::onSettingsActionsChanged);
    connect(local, &LocalWidget::sig_connectHost, this, &AndroidMainWindow::onConnectHost);
    connect(remote, &RemoteWidget::sig_connectHost, this, &AndroidMainWindow::onConnectRouterHost);
    connect(app_bar_, &AppBar::sig_backClicked, this, &AndroidMainWindow::onBackClicked);
    connect(app_bar_, &AppBar::sig_searchTextChanged, this, &AndroidMainWindow::onSearchTextChanged);

    app_bar_->setSearchMaxLength(static_cast<int>(proto::router::kMaxHostQueryLength));

    nav_bar_->addItem(tr("Local"), ":/img/folder.svg");
    nav_bar_->addItem(tr("Remote"), ":/img/workspace.svg");
    nav_bar_->addItem(tr("Router"), ":/img/stack.svg");
    nav_bar_->addItem(tr("Settings"), ":/img/settings.svg");

    // The address book shell. While a desktop connection is active it is replaced by the desktop view.
    shell_ = new QWidget(this);
    QVBoxLayout* shell_layout = new QVBoxLayout(shell_);
    shell_layout->setContentsMargins(0, 0, 0, 0);
    shell_layout->setSpacing(0);
    shell_layout->addWidget(app_bar_);
    shell_layout->addWidget(stack_content_, 1);
    shell_layout->addWidget(nav_bar_);

    stack_root_ = new QStackedWidget(this);
    stack_root_->addWidget(shell_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_root_);

    KeyboardInset* keyboard_inset = new KeyboardInset(this, shell_);
    connect(keyboard_inset, &KeyboardInset::sig_activeChanged, this, &AndroidMainWindow::onKeyboardActiveChanged);

    connect(nav_bar_, &BottomNavigationBar::sig_currentChanged,
            this, &AndroidMainWindow::onSectionChanged);

#if !defined(ASPIA_GOOGLE_PLAY)
    connect(GuiApplication::findWorker<UpdateWorker>(), &UpdateWorker::sig_updateAvailable,
            this, [this](const UpdateInfo& update_info)
    {
        // The gate of the master password is still on the screen at this point, and a question
        // asked over it comes before the application is even unlocked.
        if (!unlocked_)
        {
            pending_update_ = update_info;
            return;
        }

        showUpdatePrompt(update_info);
    }, Qt::QueuedConnection);
#endif // !defined(ASPIA_GOOGLE_PLAY)

    onSectionChanged(nav_bar_->currentIndex());

    connect(GuiApplication::instance(), &QGuiApplication::applicationStateChanged,
            this, &AndroidMainWindow::onApplicationStateChanged);

    // The gate runs once the event loop is active so the window is laid out and the dialog scrim
    // covers it.
    QMetaObject::invokeMethod(this, [this]() { runMasterPasswordGate(); }, Qt::QueuedConnection);

    // The system Back is taken before it reaches the focused widget, see eventFilter().
    qApp->installEventFilter(this);
}

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::~AndroidMainWindow()
{
    delete two_factor_dialog_;
}

//--------------------------------------------------------------------------------------------------
bool AndroidMainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Only the system Back that comes to this window. It is taken here, before the focused widget: a
    // line edit shows the on-screen keyboard on the release of any key, and the desktop view sends the
    // keys to the host.
    if (watched != windowHandle() ||
        (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) ||
        static_cast<QKeyEvent*>(event)->key() != Qt::Key_Back)
    {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress)
    {
        // The system Back nobody handles closes the window on the release, and with it the application.
        // Taking the press keeps Qt from doing that; only the root page of a section lets it through.
        if (!visibleOverlay() && stack_root_->currentWidget() == shell_ && !app_bar_->isBackVisible())
        {
            back_press_ = BackPress::PASSED;
            return QWidget::eventFilter(watched, event);
        }

        back_press_ = BackPress::TAKEN;
        return true;
    }

    const BackPress back_press = back_press_;
    back_press_ = BackPress::NONE;

    // While the on-screen keyboard is up, the press goes to the keyboard, which hides itself, and only
    // the release may come here. It is dropped, otherwise the line edit shows the keyboard again.
    if (back_press == BackPress::NONE)
        return true;

    if (back_press == BackPress::PASSED)
        return QWidget::eventFilter(watched, event);

    // The action follows on the release, so the whole press is over before the screen changes. An
    // overlay is closed as with a tap outside of it.
    if (QWidget* overlay = visibleOverlay())
    {
        if (QDialog* dialog = qobject_cast<QDialog*>(overlay))
            dialog->reject();
        else if (BottomSheet* sheet = qobject_cast<BottomSheet*>(overlay))
            sheet->dismiss();
        else
            overlay->close();
        return true;
    }

    // A session screen returns the same way as with the back button of its app bar. The desktop has
    // none: a stray swipe must not drop the connection, so its actions are opened, Disconnect among
    // them.
    QWidget* screen = stack_root_->currentWidget();
    if (screen == desktop_)
        desktop_->onShowActions();
    else if (screen == file_transfer_)
        onFileTransferClosed();
    else if (screen == chat_)
        onChatClosed();
    else if (screen == authorization_)
        onAuthorizationClosed();
    else
        onBackClicked();

    return true;
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSectionChanged(int index)
{
    stack_content_->setCurrentIndex(index);

    // The back button belongs to the remote host view; any tab switch clears it.
    app_bar_->setBackVisible(false);

    RouterWidget* router_widget = qobject_cast<RouterWidget*>(stack_content_->widget(SECTION_ROUTER));
    if (index != SECTION_ROUTER && router_widget)
        router_widget->resetToList();

    LocalWidget* local = qobject_cast<LocalWidget*>(stack_content_->widget(SECTION_LOCAL));
    RemoteWidget* remote = qobject_cast<RemoteWidget*>(stack_content_->widget(SECTION_REMOTE));
    SettingsWidget* settings = qobject_cast<SettingsWidget*>(stack_content_->widget(SECTION_SETTINGS));

    if (index != SECTION_SETTINGS && settings)
        settings->resetToSettings();

    // Refresh from storage when a browsing screen becomes visible (picks up changes made
    // elsewhere). This also brings the widget back to its tree page, which is what the app bar
    // actions are asked for below.
    if (index == SECTION_LOCAL && local)
        local->reload();
    else if (index == SECTION_REMOTE && remote)
        remote->reload();
    else if (index == SECTION_ROUTER && router_widget)
        router_widget->reload();

    QList<QWidget*> actions;
    if (index == SECTION_LOCAL && local)
        actions = local->appBarActions();
    else if (index == SECTION_ROUTER && router_widget)
        actions = router_widget->appBarActions();
    else if (index == SECTION_REMOTE && remote)
        actions = remote->appBarActions();
    else if (index == SECTION_SETTINGS && settings)
        actions = settings->appBarActions();
    app_bar_->setActions(actions);

    switch (index)
    {
        case SECTION_LOCAL:
            app_bar_->setTitle(tr("Local"));
            break;

        case SECTION_REMOTE:
            app_bar_->setTitle(tr("Remote"));
            break;

        case SECTION_ROUTER:
            app_bar_->setTitle(tr("Router"));
            break;

        case SECTION_SETTINGS:
            app_bar_->setTitle(tr("Settings"));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onKeyboardActiveChanged(bool active)
{
    nav_bar_->setVisible(!active);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRouterActionsChanged()
{
    RouterWidget* router_widget = qobject_cast<RouterWidget*>(stack_content_->widget(SECTION_ROUTER));
    if (nav_bar_->currentIndex() == SECTION_ROUTER && router_widget)
        app_bar_->setActions(router_widget->appBarActions());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onLocalActionsChanged()
{
    LocalWidget* local = qobject_cast<LocalWidget*>(stack_content_->widget(SECTION_LOCAL));
    if (nav_bar_->currentIndex() == SECTION_LOCAL && local)
        app_bar_->setActions(local->appBarActions());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onLocalTitleChanged(const QString& title, bool back_visible)
{
    if (nav_bar_->currentIndex() != SECTION_LOCAL)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Local") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRouterTitleChanged(const QString& title, bool back_visible)
{
    if (nav_bar_->currentIndex() != SECTION_ROUTER)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Router") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRemoteTitleChanged(const QString& title, bool back_visible)
{
    if (nav_bar_->currentIndex() != SECTION_REMOTE)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Remote") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSettingsTitleChanged(const QString& title, bool back_visible)
{
    if (nav_bar_->currentIndex() != SECTION_SETTINGS)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Settings") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSettingsActionsChanged()
{
    SettingsWidget* settings = qobject_cast<SettingsWidget*>(stack_content_->widget(SECTION_SETTINGS));
    if (nav_bar_->currentIndex() == SECTION_SETTINGS && settings)
        app_bar_->setActions(settings->appBarActions());
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
        onSectionChanged(nav_bar_->currentIndex());
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSearchTextChanged(const QString& text)
{
    switch (nav_bar_->currentIndex())
    {
        case SECTION_LOCAL:
            if (LocalWidget* local = qobject_cast<LocalWidget*>(stack_content_->widget(SECTION_LOCAL)))
                local->searchQuery(text);
            break;

        case SECTION_REMOTE:
            if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(stack_content_->widget(SECTION_REMOTE)))
                remote->searchQuery(text);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onBackClicked()
{
    switch (nav_bar_->currentIndex())
    {
        case SECTION_LOCAL:
            if (LocalWidget* local = qobject_cast<LocalWidget*>(stack_content_->widget(SECTION_LOCAL)))
                local->goBack();
            break;

        case SECTION_REMOTE:
            if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(stack_content_->widget(SECTION_REMOTE)))
                remote->goBack();
            break;

        case SECTION_ROUTER:
            if (RouterWidget* router_widget = qobject_cast<RouterWidget*>(stack_content_->widget(SECTION_ROUTER)))
                router_widget->goBack();
            break;

        case SECTION_SETTINGS:
            if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(stack_content_->widget(SECTION_SETTINGS)))
                settings->goBack();
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onConnectHost(qint64 entry_id, proto::peer::SessionType session_type)
{
    LocalHostConfig entry;
    const Database::FindResult found = LocalHostStore().find(entry_id, &entry);
    if (found != Database::FindResult::FOUND)
    {
        LOG(ERROR) << "Unable to read host" << entry_id;

        if (found == Database::FindResult::UNREADABLE)
        {
            MessageDialog::info(this, tr("Connection"),
                tr("The data of the host is damaged. Edit the host and enter it again."));
        }
        return;
    }

    openSession(HostConfig::forLocalHost(entry), session_type);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onConnectRouterHost(const HostConfig& host, proto::peer::SessionType session_type)
{
    openSession(host, session_type);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onDesktopClosed()
{
    if (!desktop_)
        return;

    stack_root_->setCurrentWidget(shell_);
    stack_root_->removeWidget(desktop_);
    desktop_->deleteLater();
    desktop_ = nullptr;

    setDrawIntoCutout(false);
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, true);
    showMaximized();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onFileTransferClosed()
{
    if (!file_transfer_)
        return;

    stack_root_->setCurrentWidget(shell_);
    stack_root_->removeWidget(file_transfer_);
    file_transfer_->deleteLater();
    file_transfer_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onChatClosed()
{
    if (!chat_)
        return;

    stack_root_->setCurrentWidget(shell_);
    stack_root_->removeWidget(chat_);
    chat_->deleteLater();
    chat_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onAuthorizationAccepted()
{
    if (!authorization_)
        return;

    const HostConfig host = authorization_->host();
    const proto::peer::SessionType session_type = authorization_->sessionType();
    const bool save_credentials = authorization_->isSaveCredentialsChecked();
    const qint64 credential_id = authorization_->credentialId();

    onAuthorizationClosed();

    // What this screen writes itself is what it takes back if the host refuses it. A one-time
    // password leaves the user name empty and is good for one connection.
    bool credentials_saved = false;
    if (save_credentials && !host.username().isEmpty())
    {
        saveHostCredentials(host, credential_id);
        credentials_saved = true;
    }

    startSession(host, session_type, credentials_saved);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onAuthorizationClosed()
{
    if (!authorization_)
        return;

    stack_root_->setCurrentWidget(shell_);
    stack_root_->removeWidget(authorization_);
    authorization_->deleteLater();
    authorization_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onTwoFactorRequired()
{
    // Being asked again, by a fresh question or by the button of the card, takes the dismissal
    // back, even when another dialog holds the screen right now.
    two_factor_dismissed_ = false;

    // The lock screen owns the display: both are overlays of the same window, so a dialog
    // opened now would rise above the master password prompt, and an enrollment would put the
    // new setup key in front of whoever holds the phone. The question waits for the unlock.
    if (relocking_)
        return;

    if (two_factor_dialog_)
        return;

    // The question of the router. Gone or blocked means nothing to put on screen.
    QPointer<TwoFactorPrompt> prompt(RouterController::twoFactorPrompt());
    if (!prompt || prompt->blockedSeconds() > 0)
        return;

    TwoFactorDialog* dialog =
        new TwoFactorDialog(prompt->otpauthUri(), prompt->codeRefused(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    two_factor_dialog_ = dialog;

    connect(dialog, &QDialog::finished, this, [this, prompt, dialog](int result)
    {
        two_factor_dialog_.clear();

        if (result == QDialog::Accepted && prompt)
            prompt->submitCode(dialog->code());
        else if (prompt)
            two_factor_dismissed_ = true;

        // A question that replaced the answered one takes the screen. A dismissed question
        // steps aside until it is asked again.
        showNextTwoFactorPrompt();
    });

    // The question can leave the screen before the operator does: its death (the router left,
    // the challenge changed) closes the dialog.
    connect(prompt, &QObject::destroyed, dialog, &QWidget::close);

    // open() and not exec(): the prompt is modal, so it cannot be missed, but it does not spin a
    // nested event loop inside the delivery of the challenge that opened it.
    dialog->open();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationActive)
    {
        // Back in the foreground: re-lock if the background timeout has elapsed.
        if (relocking_ || !MasterPassword::isSet() || background_since_ == TimePoint())
            return;

        const TimePoint since = background_since_;
        background_since_ = TimePoint();

        if (Clock::now() - since >= kLockTimeout)
            relock();
    }
    else if (!relocking_ && background_since_ == TimePoint())
    {
        // Leaving the foreground: remember when, ignoring the transitions caused by the lock prompt.
        background_since_ = Clock::now();
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::showNextTwoFactorPrompt()
{
    if (two_factor_dismissed_)
        return;

    TwoFactorPrompt* prompt = RouterController::twoFactorPrompt();
    if (prompt && prompt->blockedSeconds() == 0)
        onTwoFactorRequired();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::runMasterPasswordGate()
{
    const MasterPasswordDialog::Mode mode = MasterPassword::isSet() ?
        MasterPasswordDialog::Mode::UNLOCK : MasterPasswordDialog::Mode::CREATE;

    // The prompt itself sends the app through the background states (the fingerprint sheet), so guard
    // against the timer treating that as a real background trip.
    relocking_ = true;

    // The data cryptor is invalid until the dialog unlocks it, so quit if the user cancels. The
    // dialog is anchored to the content stack so only it is blurred, not the bars.
    AutoQPointer<MasterPasswordDialog> dialog(new MasterPasswordDialog(mode, stack_content_));
    if (dialog->exec() != QDialog::Accepted)
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
    if (!DatabaseMigration::finish(Database::instance(), DatabaseMigration::oldFilePath()))
        LOG(ERROR) << "Unable to move the router of the old address book, the next start tries again";

    if (LocalWidget* local = qobject_cast<LocalWidget*>(stack_content_->widget(SECTION_LOCAL)))
        local->reload();

    if (RouterWidget* router_widget = qobject_cast<RouterWidget*>(stack_content_->widget(SECTION_ROUTER)))
        router_widget->reload();

    if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(stack_content_->widget(SECTION_REMOTE)))
        remote->reload();

    unlocked_ = true;

    if (pending_update_.isValid())
    {
        UpdateInfo update_info = pending_update_;
        pending_update_ = UpdateInfo();

        showUpdatePrompt(update_info);
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::showUpdatePrompt(const UpdateInfo& update_info)
{
    if (!MessageDialog::confirm(this, tr("Update"),
            tr("Version %1 is available.").arg(update_info.version().toString()), tr("Open")))
    {
        return;
    }

    nav_bar_->setCurrentIndex(SECTION_SETTINGS);

    SettingsWidget* settings = qobject_cast<SettingsWidget*>(stack_content_->widget(SECTION_SETTINGS));
    if (settings)
        settings->onShowUpdate();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::relock()
{
    relocking_ = true;

    // The data cryptor stays open; this only re-verifies the user (password or fingerprint).
    AutoQPointer<MasterPasswordDialog> dialog(
        new MasterPasswordDialog(MasterPasswordDialog::Mode::UNLOCK, stack_content_));
    if (dialog->exec() != QDialog::Accepted)
    {
        QCoreApplication::quit();
        return;
    }

    relocking_ = false;

    // The questions that arrived while the screen was locked were not shown. The first one
    // waiting takes the screen now.
    showNextTwoFactorPrompt();
}
//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openSession(HostConfig host, proto::peer::SessionType session_type)
{
    // Only a single session is supported at a time.
    if (desktop_ || file_transfer_ || chat_ || authorization_)
        return;

    if (host.entryId() > 0 && isHostId(host.address()))
    {
        RouterConfig router;
        const Database::FindResult found = RouterStore().find(&router);
        if (found != Database::FindResult::FOUND)
        {
            LOG(ERROR) << "Unable to read the router";

            QString message;
            if (found == Database::FindResult::UNREADABLE)
                message = tr("The data of the router is damaged. Edit the router and enter it again.");
            else if (found == Database::FindResult::NOT_FOUND)
                message = tr("The host is reached through a router, but no router is set. "
                             "Add a router or enter the address of the host instead of its ID.");
            else
                message = tr("Failed to read the router.");

            MessageDialog::info(this, tr("Connection"), message);
            return;
        }

        if (!router.isEnabled())
        {
            MessageDialog::info(this, tr("Connection"),
                tr("The router is turned off. Turn it on in the router settings to connect to this host."));
            return;
        }
    }

    bool can_save_credentials = host.entryId() > 0;

    if (host.entryId() <= 0 && isHostId(host.address()) && !isTempHostId(stringToHostId(host.address())))
        can_save_credentials = true;

    if (can_save_credentials)
    {
        std::optional<std::pair<QString, SecureString>> credentials = host.entryId() > 0 ?
            LocalHostStore().credentials(host.entryId()) :
            RouterHostStore().credentials(stringToHostId(host.address()));
        if (credentials.has_value())
        {
            LOG(INFO) << "Using stored credentials of host" << host.address();

            host.setUsername(credentials->first);
            host.setPassword(credentials->second);
        }
    }

    if (host.username().isEmpty() || host.password().isEmpty())
    {
        authorization_ = new AuthorizationWindow(host, session_type, can_save_credentials);
        connect(authorization_, &AuthorizationWindow::sig_accepted,
                this, &AndroidMainWindow::onAuthorizationAccepted);
        connect(authorization_, &AuthorizationWindow::sig_closed,
                this, &AndroidMainWindow::onAuthorizationClosed);

        stack_root_->addWidget(authorization_);
        stack_root_->setCurrentWidget(authorization_);
        return;
    }

    startSession(host, session_type, false);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::startSession(const HostConfig& host, proto::peer::SessionType session_type,
                                     bool credentials_saved)
{
    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            openDesktop(host, credentials_saved);
            break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            openFileTransfer(host, credentials_saved);
            break;

        case proto::peer::SESSION_TYPE_CHAT:
            openChat(host, credentials_saved);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::saveHostCredentials(const HostConfig& host, qint64 credential_id)
{
    // The pair belongs to the record, so a host entered with one keeps the link alone.
    const QString username = credential_id > 0 ? QString() : host.username();
    const SecureString password = credential_id > 0 ? SecureString() : host.password();

    if (host.entryId() > 0)
    {
        LocalHostStore store;

        LocalHostConfig local_host;
        const Database::FindResult found = store.find(host.entryId(), &local_host);
        if (found != Database::FindResult::FOUND)
        {
            LOG(ERROR) << "Unable to read local host" << host.entryId() << ":" << found;
            return;
        }

        local_host.setCredentialId(credential_id);
        local_host.setUsername(username);
        local_host.setPassword(password);

        if (!store.modify(local_host))
            LOG(ERROR) << "Unable to save credentials of local host" << host.entryId();
        return;
    }

    const HostId host_id = stringToHostId(host.address());

    RouterHostConfig credentials;
    credentials.setHostId(host_id);
    credentials.setCredentialId(credential_id);
    credentials.setUsername(username);
    credentials.setPassword(password);

    RouterHostStore store;

    RouterHostConfig stored;
    const Database::FindResult found = store.find(host_id, &stored);
    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the credentials of host" << host_id;
        return;
    }

    bool saved = false;

    if (found == Database::FindResult::NOT_FOUND)
        saved = store.add(credentials);
    else
        saved = store.modify(credentials);

    if (!saved)
    {
        LOG(ERROR) << "Unable to save credentials of host" << host_id;
        return;
    }

    RouterController::startSyncRecordExchange();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openDesktop(const HostConfig& host, bool credentials_saved)
{
    // Only a single desktop connection is supported at a time.
    if (desktop_)
        return;

    // The desktop view takes over the whole window until the connection is closed.
    desktop_ = new DesktopWindow(host, credentials_saved);
    connect(desktop_, &DesktopWindow::sig_closed, this, &AndroidMainWindow::onDesktopClosed);

    stack_root_->addWidget(desktop_);
    stack_root_->setCurrentWidget(desktop_);

    // Hide the system bars and let the desktop view fill the whole screen, including the camera
    // cutout: allow drawing into the cutout and stop the layout from reserving the safe area.
    setDrawIntoCutout(true);
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    showFullScreen();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openFileTransfer(const HostConfig& host, bool credentials_saved)
{
    // The file transfer screen is a regular page: the system bars stay visible (no full-screen or
    // cutout drawing, unlike the desktop view).
    file_transfer_ = new FileTransferWindow(host, credentials_saved);
    connect(file_transfer_, &FileTransferWindow::sig_closed, this, &AndroidMainWindow::onFileTransferClosed);

    stack_root_->addWidget(file_transfer_);
    stack_root_->setCurrentWidget(file_transfer_);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::openChat(const HostConfig& host, bool credentials_saved)
{
    chat_ = new ChatWindow(host, credentials_saved);
    connect(chat_, &ChatWindow::sig_closed, this, &AndroidMainWindow::onChatClosed);

    stack_root_->addWidget(chat_);
    stack_root_->setCurrentWidget(chat_);
}

//--------------------------------------------------------------------------------------------------
QWidget* AndroidMainWindow::visibleOverlay() const
{
    // The dialogs, sheets and menus are shown inside this window over its content; the one raised last
    // is the last child and is on top.
    const QObjectList& objects = children();
    for (auto it = objects.crbegin(); it != objects.crend(); ++it)
    {
        QWidget* widget = qobject_cast<QWidget*>(*it);
        if (!widget || !widget->isVisible())
            continue;

        if (qobject_cast<QDialog*>(widget) || qobject_cast<BottomSheet*>(widget) ||
            qobject_cast<Menu*>(widget))
        {
            return widget;
        }
    }

    return nullptr;
}

