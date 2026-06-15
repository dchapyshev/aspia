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

#include "client/android/desktop_window.h"
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

namespace {

// android.view.WindowManager.LayoutParams display cutout modes.
constexpr int kCutoutModeDefault = 0;
constexpr int kCutoutModeShortEdges = 1;

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

    content_->addWidget(local);
    content_->addWidget(remote);
    content_->addWidget(routers);
    content_->addWidget(new SettingsWidget(this));

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
    connect(local, &LocalWidget::sig_connectHost, this, &AndroidMainWindow::onConnectHost);
    connect(remote, &RemoteWidget::sig_connectHost, this, &AndroidMainWindow::onConnectRouterHost);
    connect(app_bar_, &AppBar::sig_backClicked, this, &AndroidMainWindow::onBackClicked);

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

    // The gate runs once the event loop is active so the window is laid out and the dialog scrim
    // covers it.
    QMetaObject::invokeMethod(this, [this]() { runMasterPasswordGate(); }, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::~AndroidMainWindow() = default;

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    // The language is switched from the settings combo box, and retranslation rebuilds that combo,
    // so the rebuild is deferred to avoid destroying the sender during its own signal.
    if (event->type() == QEvent::LanguageChange)
        QMetaObject::invokeMethod(this, [this]() { retranslate(); }, Qt::QueuedConnection);
}

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

    QList<QWidget*> actions;
    if (index == SECTION_LOCAL && local)
        actions = local->appBarActions();
    else if (index == SECTION_ROUTERS && routers)
        actions = routers->appBarActions();
    else if (index == SECTION_REMOTE && remote)
        actions = remote->appBarActions();
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

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onConnectHost(qint64 entry_id)
{
    std::optional<HostConfig> host = Database::instance().findHost(entry_id);
    if (!host.has_value())
        return;

    openDesktop(*host);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onConnectRouterHost(const HostConfig& host)
{
    openDesktop(host);
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
void AndroidMainWindow::runMasterPasswordGate()
{
    const MasterPasswordDialog::Mode mode = MasterPassword::isSet() ?
        MasterPasswordDialog::Mode::UNLOCK : MasterPasswordDialog::Mode::CREATE;

    // The data cryptor is invalid until the dialog unlocks it, so quit if the user cancels.
    MasterPasswordDialog dialog(mode, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        QCoreApplication::quit();
        return;
    }

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
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::retranslate()
{
    navigation_->setItemText(SECTION_LOCAL, tr("Local"));
    navigation_->setItemText(SECTION_REMOTE, tr("Remote"));
    navigation_->setItemText(SECTION_ROUTERS, tr("Routers"));
    navigation_->setItemText(SECTION_SETTINGS, tr("Settings"));

    if (LocalWidget* local = qobject_cast<LocalWidget*>(content_->widget(SECTION_LOCAL)))
        local->retranslate();

    if (RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS)))
        routers->retranslate();

    if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS)))
        settings->retranslate();

    onSectionChanged(navigation_->currentIndex());
}
