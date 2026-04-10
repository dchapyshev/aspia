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

#include "client/ui/main_window.h"

#include <QActionGroup>
#include <QDesktopServices>
#include <QLineEdit>
#include <QMessageBox>
#include <QTabBar>
#include <QUrl>
#include <QTimer>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "base/peer/host_id.h"
#include "client/ui/application.h"
#include "client/ui/settings.h"
#include "client/ui/settings_dialog.h"
#include "client/ui/client_tab.h"
#include "client/ui/chat/chat_session_window.h"
#include "client/ui/hosts/hosts_tab.h"
#include "client/ui/desktop/desktop_session_window.h"
#include "client/ui/file_transfer/file_transfer_session_window.h"
#include "client/ui/sys_info/system_info_session_window.h"
#include "common/update_checker.h"
#include "common/update_info.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "common/ui/update_dialog.h"

namespace client {

//--------------------------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    LOG(INFO) << "Ctor";

    Settings settings;

    ui.setupUi(this);

    // Create search field at the far right of the toolbar.
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui.toolbar->addWidget(spacer);

    search_field_ = new QLineEdit(this);
    search_field_->setPlaceholderText(tr("Search..."));
    search_field_->setClearButtonEnabled(true);
    search_field_->setMaximumWidth(250);
    search_action_ = ui.toolbar->addWidget(search_field_);
    search_action_->setVisible(false);

    createLanguageMenu(settings.locale());
    createThemeMenu(settings.theme());

    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    ui.action_toolbar->setChecked(settings.isToolBarEnabled());
    ui.action_statusbar->setChecked(settings.isStatusBarEnabled());
    ui.statusbar->setVisible(settings.isStatusBarEnabled());

    connect(ui.menu_language, &QMenu::triggered, this, &MainWindow::onLanguageChanged);
    connect(ui.menu_theme, &QMenu::triggered, this, &MainWindow::onThemeChanged);
    connect(ui.action_settings, &QAction::triggered, this, &MainWindow::onSettings);
    connect(ui.action_help, &QAction::triggered, this, &MainWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &MainWindow::close);
    connect(ui.toolbar, &QToolBar::visibilityChanged, ui.action_toolbar, &QAction::setChecked);
    connect(ui.action_toolbar, &QAction::toggled, ui.toolbar, &QToolBar::setVisible);
    connect(ui.action_statusbar, &QAction::toggled, ui.statusbar, &QStatusBar::setVisible);

    // Tab management.
    connect(ui.tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(ui.tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);

    // Search field.
    connect(search_field_, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);

#if defined(Q_OS_WINDOWS)
    if (settings.checkUpdates())
    {
        update_checker_ = std::make_unique<common::UpdateChecker>(settings.updateServer(), "client");

        connect(update_checker_.get(), &common::UpdateChecker::sig_checkedFinished,
                this, &MainWindow::onUpdateCheckedFinished);

        LOG(INFO) << "Start update checker";
        update_checker_->start();
    }
#endif

    connect(base::GuiApplication::instance(), &base::GuiApplication::sig_themeChanged,
            this, &MainWindow::onAfterThemeChanged);
    onAfterThemeChanged();

    // Hide dynamic menus until a tab populates them.
    ui.menu_edit->menuAction()->setVisible(false);
    ui.menu_session_type->menuAction()->setVisible(false);

    HostsTab* hosts = new HostsTab(this);
    connect(hosts, &HostsTab::sig_connect, this, &MainWindow::onConnect);

    // Create default tabs.
    addTab(hosts, tr("Hosts"), QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* /* event */)
{
    LOG(INFO) << "Close event detected";

    Settings settings;
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());
    settings.setToolBarEnabled(ui.action_toolbar->isChecked());
    settings.setStatusBarEnabled(ui.action_statusbar->isChecked());

    for (int i = 0; i < ui.tabs->count(); ++i)
    {
        ClientTab* tab = dynamic_cast<ClientTab*>(ui.tabs->widget(i));
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
        common::UpdateInfo update_info = common::UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
        }
        else
        {
            const QVersionNumber& current_version = base::kCurrentVersion;
            const QVersionNumber& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(INFO) << "New version available:" << update_version.toString();
                common::UpdateDialog(update_info, this).exec();
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
void MainWindow::onLanguageChanged(QAction* action)
{
    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();
    client::Application* application = client::Application::instance();

    LOG(INFO) << "[ACTION] Language changed:" << new_locale.toStdString();

    Settings settings;
    settings.setLocale(new_locale);
    application->setLocale(new_locale);

    ui.retranslateUi(this);

    for (QAction* theme_action : ui.menu_theme->actions())
    {
        const QString theme_id = theme_action->data().toString();
        if (!theme_id.isEmpty())
            theme_action->setText(base::GuiApplication::themeName(theme_id));
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSettings()
{
    LOG(INFO) << "[ACTION] Settings clicked";
    SettingsDialog(this).exec();
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
    common::AboutDialog(tr("Aspia Client"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onThemeChanged(QAction* action)
{
    if (!action)
        return;

    const QString theme_id = action->data().toString();
    if (theme_id.isEmpty())
        return;

    base::GuiApplication::instance()->setTheme(theme_id);

    Settings settings;
    settings.setTheme(theme_id);
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
        active_tab_->onDeactivated(ui.statusbar);
        active_tab_ = nullptr;
    }

    if (index == -1)
        return;

    ClientTab* tab = tabAt(index);
    if (!tab)
        return;

    active_tab_ = tab;
    installTabActions(active_tab_);
    active_tab_->onActivated(ui.statusbar);
    updateSearchFieldVisibility();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCloseTab(int index)
{
    ClientTab* tab = tabAt(index);
    if (!tab || !tab->isClosable())
        return;

    if (tab == active_tab_)
    {
        removeTabActions();
        tab->onDeactivated(ui.statusbar);
        active_tab_ = nullptr;
    }

    Settings settings;
    settings.setTabState(tab->objectName(), tab->saveState());

    ui.tabs->removeTab(index);
    delete tab;
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSearchTextChanged(const QString& text)
{
    if (active_tab_)
        active_tab_->onSearchTextChanged(text);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onConnect(const Config& config)
{
    if (base::isHostId(config.address_or_id) && !config.router_config.has_value())
    {
        QMessageBox::warning(this, tr("Warning"),
            tr("Connection by ID is specified in the properties of the computer, "
               "but the router is not configured. Check the parameters of the "
               "router in the properties of the address book."));
        return;
    }

    client::SessionWindow* session_window = nullptr;

    switch (config.session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            session_window = new client::DesktopSessionWindow(Settings().desktopConfig());
            break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            session_window = new client::FileTransferSessionWindow();
            break;

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            session_window = new client::SystemInfoSessionWindow();
            break;

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            session_window = new client::ChatSessionWindow();
            break;

        default:
            NOTREACHED();
            break;
    }

    if (!session_window)
        return;

    session_window->setAttribute(Qt::WA_DeleteOnClose);
    if (!session_window->connectToHost(config))
        session_window->close();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::createLanguageMenu(const QString& current_locale)
{
    Application::LocaleList locale_list = base::GuiApplication::instance()->localeList();
    if (locale_list.isEmpty())
        return;

    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : std::as_const(locale_list))
    {
        common::LanguageAction* action_language =
            new common::LanguageAction(locale.first, locale.second, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale.first)
            action_language->setChecked(true);

        ui.menu_language->addAction(action_language);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::createThemeMenu(const QString& current_theme)
{
    base::GuiApplication* app = base::GuiApplication::instance();
    const QStringList available_themes = app->availableThemes();
    QActionGroup* theme_group = new QActionGroup(this);

    theme_group->setExclusive(true);

    for (const QString& theme_id : available_themes)
    {
        QAction* action = new QAction(base::GuiApplication::themeName(theme_id), this);
        action->setCheckable(true);
        action->setData(theme_id);
        action->setActionGroup(theme_group);
        action->setChecked(theme_id == current_theme);
        ui.menu_theme->addAction(action);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::addTab(ClientTab* tab, const QString& title, const QIcon& icon)
{
    int index = ui.tabs->addTab(tab, icon, title);

    if (!tab->isClosable())
        hideCloseButtonForTab(index);

    Settings settings;
    tab->restoreState(settings.tabState(tab->objectName()));

    connect(tab, &ClientTab::sig_titleChanged, this, [this, tab](const QString& new_title)
    {
        int tab_index = ui.tabs->indexOf(tab);
        if (tab_index != -1)
            ui.tabs->setTabText(tab_index, new_title);
    });
}

//--------------------------------------------------------------------------------------------------
void MainWindow::hideCloseButtonForTab(int index)
{
    // Hide close button on both sides for cross-platform compatibility.
    QWidget* close_button = ui.tabs->tabBar()->tabButton(index, QTabBar::RightSide);
    if (close_button)
        close_button->hide();

    close_button = ui.tabs->tabBar()->tabButton(index, QTabBar::LeftSide);
    if (close_button)
        close_button->hide();
}

//--------------------------------------------------------------------------------------------------
ClientTab* MainWindow::tabAt(int index)
{
    return dynamic_cast<ClientTab*>(ui.tabs->widget(index));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::updateSearchFieldVisibility()
{
    bool show_search = active_tab_ && active_tab_->hasSearchField();

    if (!show_search && search_field_->isVisible())
        search_field_->clear();

    search_action_->setVisible(show_search);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::installTabActions(ClientTab* tab)
{
    const QList<ClientTab::ActionGroupEntry>& groups = tab->actionGroups();

    // Find the first static toolbar action to insert before it.
    QAction* before = nullptr;
    QList<QAction*> toolbar_actions = ui.toolbar->actions();
    if (!toolbar_actions.isEmpty())
        before = toolbar_actions.first();

    for (int i = 0; i < groups.size(); ++i)
    {
        const ClientTab::ActionGroupEntry& entry = groups[i];

        // Add actions to toolbar and connect visibility tracking.
        for (QAction* action : entry.second)
        {
            ui.toolbar->insertAction(before, action);
            tab_toolbar_actions_.append(action);

            connect(action, &QAction::changed, this, &MainWindow::updateSeparatorVisibility);
        }

        // Add separator after each group.
        QAction* separator = new QAction(this);
        separator->setSeparator(true);
        ui.toolbar->insertAction(before, separator);
        tab_toolbar_actions_.append(separator);

        // Add actions to corresponding menu.
        QMenu* menu = menuForActionGroup(entry.first);
        if (menu)
        {
            if (!menu->isEmpty())
                menu->addSeparator();

            menu->addActions(entry.second);
            tab_menu_actions_.append({ menu, entry.second });

            menu->menuAction()->setVisible(true);
        }
    }

    updateSeparatorVisibility();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::removeTabActions()
{
    // Remove from toolbar.
    for (QAction* action : std::as_const(tab_toolbar_actions_))
    {
        if (!action->isSeparator())
            disconnect(action, &QAction::changed, this, &MainWindow::updateSeparatorVisibility);

        ui.toolbar->removeAction(action);

        if (action->isSeparator())
            delete action;
    }

    tab_toolbar_actions_.clear();

    // Remove from menus.
    for (const auto& [menu, actions] : std::as_const(tab_menu_actions_))
    {
        for (QAction* action : actions)
            menu->removeAction(action);

        // Remove trailing separators.
        QList<QAction*> remaining = menu->actions();
        while (!remaining.isEmpty() && remaining.last()->isSeparator())
        {
            menu->removeAction(remaining.last());
            remaining.removeLast();
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
        if (!tab_toolbar_actions_[i]->isSeparator())
            continue;

        // Separator is visible if there is at least one visible action in the group before it.
        bool has_visible = false;
        for (int j = i - 1; j >= 0; --j)
        {
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
        for (const auto& [m, actions] : std::as_const(tab_menu_actions_))
        {
            if (m != menu)
                continue;
            for (QAction* action : actions)
            {
                if (action->isVisible())
                {
                    has_visible = true;
                    break;
                }
            }
            if (has_visible)
                break;
        }

        menu->menuAction()->setVisible(has_visible);
    }
}

//--------------------------------------------------------------------------------------------------
QMenu* MainWindow::menuForActionGroup(ClientTab::ActionGroup group) const
{
    switch (group)
    {
        case ClientTab::ActionGroup::EDIT:
            return ui.menu_edit;

        case ClientTab::ActionGroup::SESSION_TYPE:
            return ui.menu_session_type;

        default:
            return nullptr;
    }
}

} // namespace client
