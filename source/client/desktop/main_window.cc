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

#include "client/desktop/main_window.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QLineEdit>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QWindow>

#include <optional>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "base/peer/host_id.h"
#include "client/database.h"
#include "client/host_url.h"
#include "client/router.h"
#include "client/settings.h"
#include "client/desktop/client_tab.h"
#include "client/desktop/client_window.h"
#include "client/desktop/management_tab.h"
#include "client/desktop/settings_tab.h"
#include "client/desktop/tab_bar.h"
#include "client/desktop/tab_widget.h"
#include "client/desktop/chat/chat_window.h"
#include "client/desktop/desktop/desktop_window.h"
#include "client/desktop/file_transfer/file_transfer_window.h"
#include "client/desktop/management/search_dialog.h"
#include "client/desktop/sys_info/system_info_window.h"
#include "client/desktop/terminal/terminal_window.h"
#include "common/update_checker.h"
#include "common/update_info.h"
#include "common/desktop/about_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/session_type.h"
#include "common/desktop/update_dialog.h"
#include "proto/peer.h"
#include "ui_main_window.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString escapeTabTitle(const QString& title)
{
    QString result = title;
    result.replace('&', "&&");
    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(std::make_unique<Ui::MainWindow>())
{
    LOG(INFO) << "Ctor";

    Settings settings;

    ui->setupUi(this);

    connect(ui->tabs->tabBar(), &TabBar::sig_tabDetachRequested, this, &MainWindow::onTabDetachRequested);

    // Create search field at the far right of the toolbar.
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->toolbar->addWidget(spacer);

    search_field_ = new QLineEdit(this);
    search_field_->setPlaceholderText(tr("Search..."));
    search_field_->setClearButtonEnabled(true);
    search_field_->setMaximumWidth(250);
    search_action_ = ui->toolbar->addWidget(search_field_);

    bool search_field_enabled = settings.isSearchFieldEnabled();
    ui->action_search_field->setChecked(search_field_enabled);
    search_action_->setVisible(search_field_enabled);

    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    ui->action_toolbar->setChecked(settings.isToolBarEnabled());
    ui->action_statusbar->setChecked(settings.isStatusBarEnabled());
    ui->statusbar->setVisible(settings.isStatusBarEnabled());

    bool large_icons = settings.largeIcons();
    ui->toolbar->setIconSize(large_icons ? QSize(32, 32) : QSize(24, 24));
    ui->action_large_icons->setChecked(large_icons);

    ui->action_sessions_in_tabs->setChecked(settings.openSessionsInTabs());

    const bool always_on_top = settings.alwaysOnTop();
    ui->action_always_on_top->setChecked(always_on_top);
    setWindowFlag(Qt::WindowStaysOnTopHint, always_on_top);
    connect(ui->action_always_on_top, &QAction::toggled, this, &MainWindow::onAlwaysOnTop);

    connect(ui->action_settings, &QAction::triggered, this, &MainWindow::onSettings);
    connect(ui->action_help, &QAction::triggered, this, &MainWindow::onHelp);
    connect(ui->action_about, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui->action_exit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->toolbar, &QToolBar::visibilityChanged, ui->action_toolbar, &QAction::setChecked);
    connect(ui->action_toolbar, &QAction::toggled, ui->toolbar, &QToolBar::setVisible);
    connect(ui->action_statusbar, &QAction::toggled, this, &MainWindow::updateStatusBarVisibility);
    connect(ui->action_large_icons, &QAction::toggled, this, [this](bool enable)
    {
        ui->toolbar->setIconSize(enable ? QSize(32, 32) : QSize(24, 24));
    });
    connect(ui->action_search_field, &QAction::toggled, this, [this](bool enable)
    {
        // Clear any active query when hiding the field so we don't get stuck in search mode.
        if (!enable)
            search_field_->clear();
        else if (search_dialog_)
            search_dialog_->close();
        search_action_->setVisible(enable);
    });

    // Tab management.
    connect(ui->tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(ui->tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);

    // Search field.
    connect(search_field_, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);

    QShortcut* find_shortcut = new QShortcut(QKeySequence::Find, this);
    connect(find_shortcut, &QShortcut::activated, this, &MainWindow::onFindAction);

#if defined(Q_OS_WINDOWS)
    Database& db = Database::instance();
    if (db.isCheckUpdatesEnabled())
    {
        QString update_server = db.updateServer();
        update_checker_ = std::make_unique<UpdateChecker>(update_server, "client");

        connect(update_checker_.get(), &UpdateChecker::sig_checkedFinished,
                this, &MainWindow::onUpdateCheckedFinished);

        LOG(INFO) << "Start update checker";
        update_checker_->start();
    }
#endif

    connect(GuiApplication::instance(), &GuiApplication::sig_themeChanged,
            this, &MainWindow::onAfterThemeChanged);
    onAfterThemeChanged();

    // Hide dynamic menus until a tab populates them.
    ui->menu_edit->menuAction()->setVisible(false);
    ui->menu_session_type->menuAction()->setVisible(false);
    ui->menu_action->menuAction()->setVisible(false);

    // Create default tabs.
    addTab(new ManagementTab(this), tr("Management"), QIcon(":/img/cms.svg"));
}

//--------------------------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void MainWindow::showAndActivate()
{
    LOG(INFO) << "Show and activate window";

    show();
    raise();
    activateWindow();
    setFocus();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::connectToUrl(const QString& url)
{
    LOG(INFO) << "Connect to URL:" << url;
    showAndActivate();

    HostUrl host_url = HostUrl::fromString(url);
    if (!host_url.isValid())
    {
        MsgBox::warning(this, tr("Invalid link \"%1\".").arg(url));
        return;
    }

    Database& db = Database::instance();
    HostConfig host;

    if (host_url.isRouterHost())
    {
        if (!db.findRouter(host_url.routerId()).has_value())
        {
            MsgBox::warning(this,
                tr("The router referenced by the link was not found in the address book."));
            return;
        }

        // The credentials of a router host live in its address book record on the router, so
        // fetch the record before connecting. If the router is not connected, connect right
        // away and let the authorization dialog ask for the credentials.
        Router* router = Router::instance(host_url.routerId());
        if (router && router->status() == Router::Status::ONLINE)
        {
            qint64 router_id = host_url.routerId();
            HostId host_id = host_url.hostId();
            proto::peer::SessionType session_type = host_url.sessionType();

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

                onConnect(host, session_type);
            });
            return;
        }

        host.setRouterId(host_url.routerId());
        host.setAddress(hostIdToString(host_url.hostId()));
    }
    else
    {
        std::optional<HostConfig> entry = db.findHost(host_url.entryId());
        if (!entry.has_value())
        {
            MsgBox::warning(this,
                tr("The host referenced by the link was not found in the address book."));
            return;
        }

        host = *entry;

        if (host.routerId() != 0 && !db.findRouter(host.routerId()).has_value())
        {
            MsgBox::warning(this, tr("The router associated with this host has been deleted. "
                                     "Edit the host to select another router or switch to "
                                     "direct connection."));
            return;
        }

        db.setConnectTime(host.id(), QDateTime::currentSecsSinceEpoch());
    }

    onConnect(host, host_url.sessionType());
}

//--------------------------------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* /* event */)
{
    LOG(INFO) << "Close event detected";

    Settings settings;
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());
    settings.setToolBarEnabled(ui->action_toolbar->isChecked());
    settings.setStatusBarEnabled(ui->action_statusbar->isChecked());
    settings.setSearchFieldEnabled(ui->action_search_field->isChecked());
    settings.setLargeIcons(ui->action_large_icons->isChecked());
    settings.setOpenSessionsInTabs(ui->action_sessions_in_tabs->isChecked());
    settings.setAlwaysOnTop(ui->action_always_on_top->isChecked());

    for (int i = 0; i < ui->tabs->count(); ++i)
    {
        Tab* tab = dynamic_cast<Tab*>(ui->tabs->widget(i));
        if (tab)
            settings.setTabState(tab->objectName(), tab->saveState());
    }

    QApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onUpdateCheckedFinished(const QByteArray& result)
{
    if (result.isEmpty())
    {
        LOG(ERROR) << "Error while retrieving update information";
    }
    else
    {
        UpdateInfo update_info = UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
        }
        else
        {
            const QVersionNumber& current_version = kCurrentVersion;
            const QVersionNumber& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(INFO) << "New version available:" << update_version.toString();
                UpdateDialog(update_info, this).exec();
            }
        }
    }

    QTimer::singleShot(0, this, [this]()
    {
        LOG(INFO) << "Destroy update checker";
        update_checker_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSettings()
{
    LOG(INFO) << "[ACTION] Settings clicked";

    // If a settings tab is already open, just activate it.
    for (int i = 0; i < ui->tabs->count(); ++i)
    {
        Tab* tab = tabAt(i);
        if (tab && tab->tabType() == Tab::Type::SETTINGS)
        {
            ui->tabs->setCurrentIndex(i);
            return;
        }
    }

    SettingsTab* settings_tab = new SettingsTab(this);

    connect(settings_tab, &SettingsTab::sig_desktopConfigChanged, this, [this]()
    {
        for (int i = 0; i < ui->tabs->count(); ++i)
        {
            ClientTab* client_tab = dynamic_cast<ClientTab*>(ui->tabs->widget(i));
            if (client_tab)
            {
                if (ClientWindow* client_window = client_tab->clientWindow())
                    client_window->applySettings();
            }
        }
    });

    addTab(settings_tab, tr("Settings"), QIcon(":/img/settings.svg"));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onHelp()
{
    LOG(INFO) << "[ACTION] Help clicked";
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAbout()
{
    LOG(INFO) << "[ACTION] About clicked";
    AboutDialog(tr("Aspia Client"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAfterThemeChanged()
{

}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCurrentTabChanged(int index)
{
    if (active_tab_)
    {
        removeTabActions();
        active_tab_->deactivate(ui->statusbar);
        active_tab_ = nullptr;
    }

    if (index == -1)
        return;

    Tab* tab = tabAt(index);
    if (!tab)
        return;

    active_tab_ = tab;
    installTabActions(active_tab_);
    active_tab_->activate(ui->statusbar);
    syncSearchField();
    updateStatusBarVisibility();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCloseTab(int index)
{
    Tab* tab = tabAt(index);
    if (!tab || !tab->isClosable())
        return;

    if (tab == active_tab_)
    {
        removeTabActions();
        tab->deactivate(ui->statusbar);
        active_tab_ = nullptr;
    }

    Settings settings;
    settings.setTabState(tab->objectName(), tab->saveState());

    ui->tabs->removeTab(index);
    delete tab;
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSearchTextChanged(const QString& text)
{
    ManagementTab* management_tab = managementTab();
    if (!management_tab)
        return;

    // Always route the search to the hosts tab.
    management_tab->searchTextChanged(text);

    // Switch to the hosts tab when the user starts searching from another tab.
    if (active_tab_ != management_tab && !text.isEmpty())
    {
        int index = ui->tabs->indexOf(management_tab);
        if (index != -1)
            ui->tabs->setCurrentIndex(index);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onFindAction()
{
    if (!managementTab())
        return;

    if (ui->action_search_field->isChecked())
    {
        // Toolbar search field is visible - just move the input focus to it.
        search_field_->setFocus();
        search_field_->selectAll();
    }
    else
    {
        // Toolbar search field is hidden - offer a dedicated search dialog instead.
        showSearchDialog();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onConnect(const HostConfig& host, proto::peer::SessionType session_type)
{
    if (isHostId(host.address()) && host.routerId() <= 0)
    {
        MsgBox::warning(this,
            tr("Connection by ID is specified in the properties of the host, "
               "but the router is not configured. Check the parameters of the "
               "router in the properties of the address book."));
        return;
    }

    // For sessions launched from another session (e.g. file transfer started from a desktop
    // session), inherit the tabbed/detached mode of the parent tab. Otherwise (ManagementTab or
    // direct call) fall back to the global "open sessions in tabs" setting.
    Tab* parent_tab = qobject_cast<Tab*>(sender());
    bool detached = (parent_tab && parent_tab->tabType() == Tab::Type::SESSION) ?
        parent_tab->isDetached() : !ui->action_sessions_in_tabs->isChecked();

    ClientWindow* client_window = nullptr;

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
        {
            proto::control::Config desktop_config = Settings().desktopConfig();

            // When the session opens in a tab, its visible area is bounded by the main window from
            // the very first frame. Pass the initial preferred size in the config so the host
            // scales the desktop to fit the tab right away, instead of streaming the full
            // resolution until the first resize event.
            if (!detached)
            {
                if (QWidget* content = ui->tabs->currentWidget())
                {
                    qreal scale = devicePixelRatioF();
                    proto::control::Size* preferred_size = desktop_config.mutable_preferred_size();
                    preferred_size->set_width(static_cast<int>(content->width() * scale));
                    preferred_size->set_height(static_cast<int>(content->height() * scale));
                }
            }

            client_window = new DesktopWindow(desktop_config);
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            client_window = new FileTransferWindow();
            break;

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            client_window = new SystemInfoWindow();
            break;

        case proto::peer::SESSION_TYPE_CHAT:
            client_window = new ChatWindow();
            break;

        case proto::peer::SESSION_TYPE_TERMINAL:
            client_window = new TerminalWindow();
            break;

        default:
            NOTREACHED();
            break;
    }

    if (!client_window)
        return;

    QString display_name = Database::instance().displayName();
    QString computer_name = host.name().isEmpty() ? host.address() : host.name();
    QIcon icon = sessionIcon(session_type);

    client_window->setWindowIcon(icon);

    // Keep the session window owned by us during connection so that only the status dialog is
    // shown. The session tab is created later, once the connection has been established.
    client_window->setParent(this);

    auto connected = std::make_shared<bool>(false);

    connect(client_window, &ClientWindow::sig_connected, this,
        [this, client_window, computer_name, icon, detached, connected]()
    {
        if (*connected)
            return;
        *connected = true;

        ClientTab* client_tab = new ClientTab(client_window);
        addTab(client_tab, computer_name, icon);

        if (detached)
        {
            int index = ui->tabs->indexOf(client_tab);
            ui->tabs->tabBar()->setTabVisible(index, false);
            client_tab->detachToWindow();
        }
    });

    // The session ended before it was ever connected (authorization cancelled or a connection
    // error dismissed by the user). No tab was created, so just discard the window.
    connect(client_window, &ClientWindow::sig_stop, this, [client_window, connected]()
    {
        if (!*connected)
            client_window->deleteLater();
    });

    if (!client_window->connectToHost(host, display_name))
        client_window->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabDetachRequested(int index, const QPoint& global_pos)
{
    Tab* tab = tabAt(index);
    if (!tab || !tab->isDetachable() || tab->isDetached())
        return;

    QSize new_size = tab->size() / 2;

    tab->detachToWindow();
    ui->tabs->tabBar()->setTabVisible(index, false);

    QWidget* window = tab->detachedWindow();
    if (!window)
        return;

    window->resize(new_size);

    int title_h = window->style()->pixelMetric(QStyle::PM_TitleBarHeight);
    window->move(global_pos - QPoint(title_h + title_h / 2, title_h / 2));
    window->raise();
    window->activateWindow();

    if (QWindow* handle = window->windowHandle())
        handle->startSystemMove();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabDragMove(const QPoint& global_pos)
{
    Tab* tab = qobject_cast<Tab*>(sender());
    if (!tab || !tab->isDetached())
        return;

    int index = ui->tabs->indexOf(tab);
    if (index == -1)
        return;

    TabBar* tabbar = ui->tabs->tabBar();
    bool over = tabBarHitTest(global_pos);
    if (tabbar->isTabVisible(index) != over)
        tabbar->setTabVisible(index, over);
    tabbar->setDropTarget(over ? index : -1);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabDragFinished(const QPoint& global_pos)
{
    Tab* tab = qobject_cast<Tab*>(sender());
    if (!tab || !tab->isDetached())
        return;

    int index = ui->tabs->indexOf(tab);
    if (index == -1)
        return;

    TabBar* tabbar = ui->tabs->tabBar();
    tabbar->setDropTarget(-1);

    if (tabBarHitTest(global_pos))
    {
        tab->attachToTab();
        tabbar->setTabVisible(index, true);
        ui->tabs->setCurrentIndex(index);
    }
    else if (tabbar->isTabVisible(index))
    {
        // Drop missed the tab bar; clean up the preview state we may have set during drag.
        tabbar->setTabVisible(index, false);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabFullscreenRequested(bool enabled)
{
    Tab* tab = qobject_cast<Tab*>(sender());
    if (!tab || !tab->isDetachable())
        return;

    int index = ui->tabs->indexOf(tab);
    if (index == -1)
        return;

    TabBar* tabbar = ui->tabs->tabBar();

    if (enabled)
    {
        tab->setVisibleBeforeFullscreen(tabbar->isTabVisible(index));

        if (!tab->isDetached())
            tab->detachToWindow();
        tabbar->setTabVisible(index, false);

        if (QWidget* window = tab->detachedWindow())
        {
            if (QScreen* target_screen = screen())
                window->move(target_screen->geometry().topLeft());
            window->showFullScreen();
        }
    }
    else
    {
        if (QWidget* window = tab->detachedWindow())
            window->showNormal();

        if (tab->isVisibleBeforeFullscreen())
        {
            tab->attachToTab();
            tabbar->setTabVisible(index, true);
            ui->tabs->setCurrentIndex(index);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabMinimizeRequested()
{
    Tab* tab = qobject_cast<Tab*>(sender());
    if (!tab || !tab->isDetached())
        return;

    if (QWidget* window = tab->detachedWindow())
        window->showMinimized();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabShowRequested()
{
    Tab* tab = qobject_cast<Tab*>(sender());
    if (!tab)
        return;

    int index = ui->tabs->indexOf(tab);
    if (index == -1)
        return;

    if (tab->isDetached())
    {
        if (QWidget* window = tab->detachedWindow())
        {
            window->showNormal();
            window->activateWindow();
        }
    }
    else if (ui->tabs->tabBar()->isTabVisible(index))
    {
        ui->tabs->setCurrentIndex(index);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAlwaysOnTop(bool checked)
{
    auto apply = [checked](QWidget* widget)
    {
        const bool was_visible = widget->isVisible();
        widget->setWindowFlag(Qt::WindowStaysOnTopHint, checked);
        if (was_visible)
            widget->show();
    };

    apply(this);

    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (widget != this && qobject_cast<ClientWindow*>(widget))
            apply(widget);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::addTab(Tab* tab, const QString& title, const QIcon& icon)
{
    int index = ui->tabs->addTab(tab, icon, escapeTabTitle(title));

    if (!tab->isClosable())
        hideCloseButtonForTab(index);

    Settings settings;
    tab->restoreState(settings.tabState(tab->objectName()));

    connect(tab, &Tab::sig_titleChanged, this, [this, tab](const QString& new_title)
    {
        int tab_index = ui->tabs->indexOf(tab);
        if (tab_index != -1)
            ui->tabs->setTabText(tab_index, escapeTabTitle(new_title));
    });

    connect(tab, &Tab::sig_closeRequested, this, [this, tab]()
    {
        int tab_index = ui->tabs->indexOf(tab);
        if (tab_index != -1)
            onCloseTab(tab_index);
    }, Qt::QueuedConnection);

    connect(tab, &Tab::sig_dragMove, this, &MainWindow::onTabDragMove);
    connect(tab, &Tab::sig_dragFinished, this, &MainWindow::onTabDragFinished);
    connect(tab, &Tab::sig_fullscreenRequested, this, &MainWindow::onTabFullscreenRequested);
    connect(tab, &Tab::sig_minimizeRequested, this, &MainWindow::onTabMinimizeRequested);
    connect(tab, &Tab::sig_showRequested, this, &MainWindow::onTabShowRequested);
    connect(tab, &Tab::sig_connectRequested, this, &MainWindow::onConnect);
    connect(tab, &Tab::sig_clearSearch, this, [this, tab]()
    {
        if (active_tab_ == tab)
        {
            search_field_->clear();
            if (search_dialog_)
                search_dialog_->setSearchText(QString());
        }
    });
    connect(tab, &Tab::sig_actionsChanged, this, [this, tab]()
    {
        if (active_tab_ == tab)
        {
            removeTabActions();
            installTabActions(tab);
        }
    });

    ui->tabs->setCurrentIndex(index);
}

//--------------------------------------------------------------------------------------------------
bool MainWindow::tabBarHitTest(const QPoint& global_pos) const
{
    QTabBar* tabbar = ui->tabs->tabBar();
    if (!tabbar->isVisible())
        return false;

    return tabbar->rect().contains(tabbar->mapFromGlobal(global_pos));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::hideCloseButtonForTab(int index)
{
    // Hide close button on both sides for cross-platform compatibility.
    QWidget* close_button = ui->tabs->tabBar()->tabButton(index, QTabBar::RightSide);
    if (close_button)
        close_button->hide();

    close_button = ui->tabs->tabBar()->tabButton(index, QTabBar::LeftSide);
    if (close_button)
        close_button->hide();
}

//--------------------------------------------------------------------------------------------------
Tab* MainWindow::tabAt(int index)
{
    return dynamic_cast<Tab*>(ui->tabs->widget(index));
}

//--------------------------------------------------------------------------------------------------
ManagementTab* MainWindow::managementTab() const
{
    for (int i = 0; i < ui->tabs->count(); ++i)
    {
        ManagementTab* tab = dynamic_cast<ManagementTab*>(ui->tabs->widget(i));
        if (tab)
            return tab;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void MainWindow::showSearchDialog()
{
    if (!search_dialog_)
    {
        search_dialog_ = new SearchDialog(this);
        connect(search_dialog_, &SearchDialog::sig_searchTextChanged,
                this, &MainWindow::onSearchTextChanged);
        connect(search_dialog_, &QDialog::finished, this, [this](int /* result */)
        {
            // The toolbar field is hidden, so closing the dialog is the only way back to the
            // normal view: reset the active query when it is dismissed.
            onSearchTextChanged(QString());
        });
    }

    ManagementTab* management_tab = managementTab();
    if (management_tab)
        search_dialog_->setSearchText(management_tab->searchText());

    search_dialog_->show();
    search_dialog_->raise();
    search_dialog_->activateWindow();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::syncSearchField()
{
    ManagementTab* management_tab = managementTab();
    if (!management_tab)
        return;

    QString text = management_tab->searchText();
    if (search_field_->text() != text)
    {
        QSignalBlocker blocker(search_field_);
        search_field_->setText(text);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::updateStatusBarVisibility()
{
    bool tab_has_statusbar = !active_tab_ || active_tab_->hasStatusBar();
    ui->statusbar->setVisible(ui->action_statusbar->isChecked() && tab_has_statusbar);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::installTabActions(Tab* tab)
{
    const QList<Tab::ActionGroupEntry> groups = tab->actionGroups();

    // Find the first static toolbar action to insert before it.
    QAction* before = nullptr;
    QList<QAction*> toolbar_actions = ui->toolbar->actions();
    if (!toolbar_actions.isEmpty())
        before = toolbar_actions.first();

    for (int i = 0; i < groups.size(); ++i)
    {
        const Tab::ActionGroupEntry& entry = groups[i];

        // Add actions to toolbar and connect visibility tracking. Actions marked with
        // kMenuOnlyProperty go only to the menu and are skipped here.
        bool any_in_toolbar = false;
        for (QAction* action : entry.second)
        {
            if (action->property(Tab::kMenuOnlyProperty).toBool())
                continue;

            ui->toolbar->insertAction(before, action);
            tab_toolbar_actions_.append(action);

            // Actions that carry a submenu (e.g. power control, switch session, scale) need
            // their toolbar button to open the menu on a single click.
            if (action->menu())
            {
                if (QToolButton* button =
                        qobject_cast<QToolButton*>(ui->toolbar->widgetForAction(action)))
                {
                    button->setPopupMode(QToolButton::InstantPopup);
                }
            }

            connect(action, &QAction::changed, this, &MainWindow::updateSeparatorVisibility);
            any_in_toolbar = true;
        }

        // Add separator after each group (only if at least one action was added to toolbar).
        if (any_in_toolbar)
        {
            QAction* separator = new QAction(this);
            separator->setSeparator(true);
            ui->toolbar->insertAction(before, separator);
            tab_toolbar_actions_.append(separator);
        }

        // Add actions to corresponding menu.
        QMenu* menu = menuForActionGroup(entry.first);
        if (menu)
        {
            // Find the first static action (the first action that is not one of our
            // previously inserted dynamic items).
            QAction* anchor = nullptr;
            const QList<QAction*> existing = menu->actions();
            for (QAction* action : existing)
            {
                bool is_ours = false;
                for (const auto& [m, items] : std::as_const(tab_menu_actions_))
                {
                    if (m == menu && items.contains(action))
                    {
                        is_ours = true;
                        break;
                    }
                }
                if (!is_ours)
                {
                    anchor = action;
                    break;
                }
            }

            QList<QPointer<QAction>> inserted_items;
            for (QAction* action : entry.second)
            {
                if (anchor)
                    menu->insertAction(anchor, action);
                else
                    menu->addAction(action);
                inserted_items.append(action);
            }

            // Separator between this dynamic group and the following items (static or next group).
            QAction* separator = anchor ? menu->insertSeparator(anchor) : menu->addSeparator();
            inserted_items.append(separator);

            tab_menu_actions_.append({ menu, inserted_items });
            menu->menuAction()->setVisible(true);
        }
    }

    updateSeparatorVisibility();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::removeTabActions()
{
    // Remove from toolbar.
    for (const QPointer<QAction>& action : std::as_const(tab_toolbar_actions_))
    {
        if (!action)
            continue;

        if (!action->isSeparator())
            disconnect(action, &QAction::changed, this, &MainWindow::updateSeparatorVisibility);

        ui->toolbar->removeAction(action);

        if (action->isSeparator())
            delete action;
    }

    tab_toolbar_actions_.clear();

    // Remove from menus.
    for (const auto& [menu, actions] : std::as_const(tab_menu_actions_))
    {
        for (const QPointer<QAction>& action : actions)
        {
            if (!action)
                continue;

            menu->removeAction(action);
            if (action->isSeparator())
                delete action;
        }

        // Hide menu if it has no actions left.
        if (menu->isEmpty())
            menu->menuAction()->setVisible(false);
    }

    tab_menu_actions_.clear();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::updateSeparatorVisibility()
{
    // Update toolbar separator visibility.
    for (int i = 0; i < tab_toolbar_actions_.size(); ++i)
    {
        if (!tab_toolbar_actions_[i] || !tab_toolbar_actions_[i]->isSeparator())
            continue;

        // Separator is visible if there is at least one visible action in the group before it.
        bool has_visible = false;
        for (int j = i - 1; j >= 0; --j)
        {
            if (!tab_toolbar_actions_[j])
                continue;
            if (tab_toolbar_actions_[j]->isSeparator())
                break;
            if (tab_toolbar_actions_[j]->isVisible())
            {
                has_visible = true;
                break;
            }
        }

        tab_toolbar_actions_[i]->setVisible(has_visible);
    }

    // Update menu visibility: hide menu if none of its tab actions are visible.
    QSet<QMenu*> menus;
    for (const auto& [menu, actions] : std::as_const(tab_menu_actions_))
        menus.insert(menu);

    for (QMenu* menu : std::as_const(menus))
    {
        bool has_visible = false;
        const QList<QAction*> menu_actions = menu->actions();
        for (QAction* action : menu_actions)
        {
            if (!action->isSeparator() && action->isVisible())
            {
                has_visible = true;
                break;
            }
        }

        menu->menuAction()->setVisible(has_visible);
    }
}

//--------------------------------------------------------------------------------------------------
QMenu* MainWindow::menuForActionGroup(Tab::ActionRole group) const
{
    switch (group)
    {
        case Tab::ActionRole::FILE:
            return ui->menu_file;

        case Tab::ActionRole::EDIT:
            return ui->menu_edit;

        case Tab::ActionRole::VIEW:
            return ui->menu_view;

        case Tab::ActionRole::ACTION:
            return ui->menu_action;

        case Tab::ActionRole::SESSION_TYPE:
            return ui->menu_session_type;

        case Tab::ActionRole::HELP:
            return ui->menu_help;

        default:
            return nullptr;
    }
}
