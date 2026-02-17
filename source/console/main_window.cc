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

#include "console/main_window.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include "base/logging.h"
#include "base/version_constants.h"
#include "client/ui/desktop/desktop_session_window.h"
#include "client/ui/file_transfer/file_transfer_session_window.h"
#include "client/ui/sys_info/system_info_session_window.h"
#include "client/ui/text_chat/text_chat_session_window.h"
#include "client/ui/router_manager/router_manager_window.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "console/address_book_tab.h"
#include "console/application.h"
#include "console/fast_connect_dialog.h"
#include "console/import_export_util.h"
#include "console/mru_action.h"
#include "console/update_settings_dialog.h"
#include "console/settings.h"
#include "common/ui/update_dialog.h"

namespace console {

//--------------------------------------------------------------------------------------------------
MainWindow::MainWindow(const QString& file_path)
{
    LOG(INFO) << "Ctor";

    Settings settings;
    Application::instance()->setAttribute(
        Qt::AA_DontShowIconsInMenus, !settings.showIconsInMenus());

    ui.setupUi(this);
    createLanguageMenu(settings.locale());

    bool large_icons = settings.largeIcons();
    ui.tool_bar->setIconSize(large_icons ? QSize(32, 32) : QSize(24, 24));
    ui.action_large_icons->setChecked(large_icons);

    bool enable_recent_open = settings.isRecentOpenEnabled();
    ui.action_remember_last->setChecked(enable_recent_open);
    if (enable_recent_open)
        mru_.setRecentOpen(settings.recentOpen());
    mru_.setPinnedFiles(settings.pinnedFiles());

    rebuildMruMenu();

    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    ui.action_show_tray_icon->setChecked(settings.alwaysShowTrayIcon());
    ui.action_minimize_to_tray->setChecked(settings.minimizeToTray());
    ui.action_toolbar->setChecked(settings.isToolBarEnabled());
    ui.action_statusbar->setChecked(settings.isStatusBarEnabled());
    ui.action_show_icons_in_menus->setChecked(settings.showIconsInMenus());

    ui.status_bar->setVisible(ui.action_statusbar->isChecked());
    showTrayIcon(ui.action_show_tray_icon->isChecked());

    connect(ui.menu_recent_open, &QMenu::triggered, this, &MainWindow::onRecentOpenTriggered);

    QTabBar* tab_bar = ui.tab_widget->tabBar();
    tab_bar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tab_bar, &QTabBar::customContextMenuRequested, this, &MainWindow::onTabContextMenu);

    connect(ui.action_show_hide, &QAction::triggered, this, &MainWindow::onShowHideToTray);
    connect(ui.action_show_tray_icon, &QAction::toggled, this, &MainWindow::showTrayIcon);
    connect(ui.action_new, &QAction::triggered, this, &MainWindow::onNew);
    connect(ui.action_open, &QAction::triggered, this, &MainWindow::onOpen);
    connect(ui.action_save, &QAction::triggered, this, &MainWindow::onSave);
    connect(ui.action_save_as, &QAction::triggered, this, &MainWindow::onSaveAs);
    connect(ui.action_save_all, &QAction::triggered, this, &MainWindow::onSaveAll);
    connect(ui.action_close, &QAction::triggered, this, &MainWindow::onClose);
    connect(ui.action_close_all, &QAction::triggered, this, &MainWindow::onCloseAll);

    connect(ui.action_address_book_properties, &QAction::triggered,
            this, &MainWindow::onAddressBookProperties);

    connect(ui.action_add_computer, &QAction::triggered, this, &MainWindow::onAddComputer);
    connect(ui.action_copy_computer, &QAction::triggered, this, &MainWindow::onCopyComputer);
    connect(ui.action_modify_computer, &QAction::triggered, this, &MainWindow::onModifyComputer);

    connect(ui.action_delete_computer, &QAction::triggered,
            this, &MainWindow::onDeleteComputer);

    connect(ui.action_add_computer_group, &QAction::triggered,
            this, &MainWindow::onAddComputerGroup);

    connect(ui.action_modify_computer_group, &QAction::triggered,
            this, &MainWindow::onModifyComputerGroup);

    connect(ui.action_delete_computer_group, &QAction::triggered,
            this, &MainWindow::onDeleteComputerGroup);

    connect(ui.action_import_computers, &QAction::triggered, this, &MainWindow::onImportComputers);
    connect(ui.action_export_computers, &QAction::triggered, this, &MainWindow::onExportComputers);

    connect(ui.action_update_status, &QAction::triggered,
            this, &MainWindow::onUpdateStatus);

    connect(ui.action_online_help, &QAction::triggered, this, &MainWindow::onOnlineHelp);
    connect(ui.action_about, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &MainWindow::close);
    connect(ui.action_fast_connect, &QAction::triggered, this, &MainWindow::onFastConnect);
    connect(ui.action_router_manage, &QAction::triggered, this, &MainWindow::connectToRouter);

    connect(ui.action_desktop_manage_connect, &QAction::triggered,
            this, &MainWindow::onDesktopManageConnect);

    connect(ui.action_desktop_view_connect, &QAction::triggered,
            this, &MainWindow::onDesktopViewConnect);

    connect(ui.action_file_transfer_connect, &QAction::triggered,
            this, &MainWindow::onFileTransferConnect);

    connect(ui.action_system_info_connect, &QAction::triggered,
            this, &MainWindow::onSystemInfoConnect);

    connect(ui.action_text_chat_connect, &QAction::triggered,
            this, &MainWindow::onTextChatConnect);

    connect(ui.action_show_icons_in_menus, &QAction::triggered, this, [=](bool enable)
    {
        Application* instance = Application::instance();
        instance->setAttribute(Qt::AA_DontShowIconsInMenus, !enable);
        Settings().setShowIconsInMenus(enable);
    });

    connect(ui.tool_bar, &QToolBar::visibilityChanged, ui.action_toolbar, &QAction::setChecked);
    connect(ui.action_toolbar, &QAction::toggled, ui.tool_bar, &QToolBar::setVisible);
    connect(ui.action_statusbar, &QAction::toggled, ui.status_bar, &QStatusBar::setVisible);
    connect(ui.tab_widget, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(ui.tab_widget, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);
    connect(ui.menu_language, &QMenu::triggered, this, &MainWindow::onLanguageChanged);
    connect(ui.action_large_icons, &QAction::toggled, this, [this](bool enable)
    {
        ui.tool_bar->setIconSize(enable ? QSize(32, 32) : QSize(24, 24));
        Settings().setLargeIcons(enable);
    });

    QActionGroup* session_type_group = new QActionGroup(this);

    session_type_group->addAction(ui.action_desktop_manage);
    session_type_group->addAction(ui.action_desktop_view);
    session_type_group->addAction(ui.action_file_transfer);
    session_type_group->addAction(ui.action_system_info);
    session_type_group->addAction(ui.action_text_chat);

    switch (settings.sessionType())
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
            ui.action_desktop_manage->setChecked(true);
            break;

        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
            ui.action_desktop_view->setChecked(true);
            break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            ui.action_file_transfer->setChecked(true);
            break;

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            ui.action_system_info->setChecked(true);
            break;

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            ui.action_text_chat->setChecked(true);
            break;

        default:
            break;
    }

    // Open all pinned files of address books.
    for (const auto& file : mru_.pinnedFiles())
    {
        if (QFile::exists(file))
        {
            addAddressBookTab(AddressBookTab::openFromFile(file, ui.tab_widget));
        }
        else
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Pinned address book file \"%1\" was not found.<br/>"
                                    "This file will be unpinned.").arg(file),
                                 QMessageBox::Ok);
            mru_.unpinFile(file);
        }
    }

    QString normalized_path(file_path);
    normalized_path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // If the address book is pinned, then it is already open.
    if (!normalized_path.isEmpty() && !mru_.isPinnedFile(normalized_path))
        addAddressBookTab(AddressBookTab::openFromFile(normalized_path, ui.tab_widget));

    if (ui.tab_widget->count() > 0)
        ui.tab_widget->setCurrentIndex(0);

#if defined(Q_OS_WINDOWS)
    connect(ui.action_check_updates, &QAction::triggered, this, &MainWindow::onCheckUpdates);
    connect(ui.action_update_settings, &QAction::triggered, this, [this]()
    {
        UpdateSettingsDialog(this).exec();
    });

    if (settings.checkUpdates())
    {
        update_checker_ = std::make_unique<common::UpdateChecker>();

        update_checker_->setUpdateServer(settings.updateServer());
        update_checker_->setPackageName("console");

        connect(update_checker_.get(), &common::UpdateChecker::sig_checkedFinished,
                this, &MainWindow::onUpdateCheckedFinished);

        update_checker_->start();
    }
#else
    ui.action_check_updates->setVisible(false);
    ui.action_update_settings->setVisible(false);
#endif
}

//--------------------------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void MainWindow::showConsole()
{
    LOG(INFO) << "Show console";

    if (tray_icon_ && isHidden())
    {
        onShowHideToTray();
    }
    else
    {
        show();
        activateWindow();
        setFocus();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::openAddressBook(const QString& file_path)
{
    LOG(INFO) << "Open address book:" << file_path;
    showConsole();

    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab)
        {
#if defined(Q_OS_WINDOWS)
            if (file_path.compare(tab->filePath(), Qt::CaseInsensitive) == 0)
#else
            if (file_path.compare(tab->filePath(), Qt::CaseSensitive) == 0)
#endif // defined(Q_OS_WINDOWS)
            {
                QMessageBox::information(this,
                                         tr("Information"),
                                         tr("Address Book \"%1\" is already open.").arg(file_path),
                                         QMessageBox::Ok);

                ui.tab_widget->setCurrentIndex(i);
                return;
            }
        }
    }

    AddressBookTab* tab = AddressBookTab::openFromFile(file_path, ui.tab_widget);
    if (!tab)
        return;

    addAddressBookTab(tab);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onNew()
{
    LOG(INFO) << "[ACTION] New address book";
    addAddressBookTab(AddressBookTab::createNew(ui.tab_widget));
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onOpen()
{
    LOG(INFO) << "[ACTION] Open address book";

    Settings settings;

    QString file_path =
        QFileDialog::getOpenFileName(this,
                                     tr("Open Address Book"),
                                     settings.lastDirectory(),
                                     tr("Aspia Address Book (*.aab)"));
    if (file_path.isEmpty())
    {
        LOG(INFO) << "No selected file path";
        return;
    }

    settings.setLastDirectory(QFileInfo(file_path).absolutePath());
    openAddressBook(file_path);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSave()
{
    LOG(INFO) << "[ACTION] Save address book";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab && tab->save())
    {
        if (Settings().isRecentOpenEnabled())
        {
            if (mru_.addRecentFile(tab->filePath()))
                rebuildMruMenu();
        }

        if (!hasChangedTabs())
            ui.action_save_all->setEnabled(false);
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSaveAs()
{
    LOG(INFO) << "[ACTION] Save as address book";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        QString old_path = tab->filePath();
        if (mru_.isPinnedFile(old_path))
        {
            std::unique_ptr<AddressBookTab> duplicate_tab(tab->duplicateTab());
            if (duplicate_tab->saveAs())
            {
                if (Settings().isRecentOpenEnabled())
                {
                    const QString& new_path = duplicate_tab->filePath();
                    if (mru_.addRecentFile(new_path))
                        rebuildMruMenu();
                }

                addAddressBookTab(duplicate_tab.release());
            }
        }
        else if (tab->saveAs())
        {
            if (Settings().isRecentOpenEnabled())
            {
                const QString& new_path = tab->filePath();
                if (mru_.addRecentFile(new_path))
                    rebuildMruMenu();
            }
        }

        if (!hasChangedTabs())
            ui.action_save_all->setEnabled(false);
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSaveAll()
{
    LOG(INFO) << "[ACTION] Save all address books";

    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
            tab->save();
    }

    if (!hasChangedTabs())
        ui.action_save_all->setEnabled(false);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onClose()
{
    LOG(INFO) << "[ACTION] Close";
    onCloseTab(ui.tab_widget->currentIndex());
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCloseAll()
{
    LOG(INFO) << "[ACTION] Close all";

    for (int i = ui.tab_widget->count(); i >= 0; --i)
        onCloseTab(i);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAddressBookProperties()
{
    LOG(INFO) << "[ACTION] Open address book properties";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->modifyAddressBook();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAddComputer()
{
    LOG(INFO) << "[ACTION] Add Computer";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->addComputer();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCopyComputer()
{
    LOG(INFO) << "[ACTION] Copy Computer";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->copyComputer();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onModifyComputer()
{
    LOG(INFO) << "[ACTION] Modify Computer";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->modifyComputer();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onDeleteComputer()
{
    LOG(INFO) << "[ACTION] Delete Computer";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->removeComputer();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAddComputerGroup()
{
    LOG(INFO) << "[ACTION] Add Computer Group";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->addComputerGroup();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onModifyComputerGroup()
{
    LOG(INFO) << "[ACTION] Modify Computer Group";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->modifyComputerGroup();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onDeleteComputerGroup()
{
    LOG(INFO) << "[ACTION] Delete Computer Group";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        tab->removeComputerGroup();
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onImportComputers()
{
    LOG(INFO) << "[ACTION] Import Computers";

    AddressBookTab* tab = currentAddressBookTab();
    if (!tab)
    {
        LOG(ERROR) << "No active tab";
        return;
    }

    proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
    if (!computer_group)
    {
        LOG(ERROR) << "Unable to get current computer group";
        return;
    }

    QString selected_filter;
    QString file_path = QFileDialog::getOpenFileName(this,
                                                     tr("Open File"),
                                                     QString(),
                                                     tr("JSON files (*.json)"),
                                                     &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "No selected file path";
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LOG(ERROR) << "Unable to open file:" << file.errorString();
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Could not open file for reading."),
                             QMessageBox::Ok);
        return;
    }

    QByteArray json_buffer = file.readAll();
    if (json_buffer.isEmpty())
    {
        LOG(ERROR) << "Empty file";
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Import file is empty."),
                             QMessageBox::Ok);
        return;
    }

    QJsonParseError parse_error;
    QJsonDocument json = QJsonDocument::fromJson(json_buffer, &parse_error);
    if (parse_error.error != QJsonParseError::NoError)
    {
        LOG(ERROR) << "Unable to parse JSON document:" << parse_error.errorString();
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Failed to parse JSON document: %1.").arg(parse_error.errorString()),
                             QMessageBox::Ok);
        return;
    }

    importComputersFromJson(json, computer_group);

    tab->reloadAll();
    tab->setChanged(true);

    LOG(INFO) << "File imported";
    QMessageBox::information(this,
                             tr("Information"),
                             tr("Import completed successfully."),
                             QMessageBox::Ok);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onExportComputers()
{
    LOG(INFO) << "[ACTION] Export Computers";

    AddressBookTab* tab = currentAddressBookTab();
    if (!tab)
    {
        LOG(ERROR) << "No active tab";
        return;
    }

    proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
    if (!computer_group)
    {
        LOG(ERROR) << "Unable to get current computer group";
        return;
    }

    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(this,
                                                     tr("Save File"),
                                                     QString(),
                                                     tr("JSON files (*.json)"),
                                                     &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "No selected file path";
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(ERROR) << "Unable to open file:" << file.errorString();
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Could not open file for writing."),
                             QMessageBox::Ok);
        return;
    }

    QJsonDocument json = exportComputersToJson(*computer_group);

    qint64 written = file.write(json.toJson());
    if (written <= 0)
    {
        LOG(ERROR) << "Unable to write file:" << file.errorString();
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Unable to write file."),
                             QMessageBox::Ok);
        return;
    }

    LOG(INFO) << "File exported";
    QMessageBox::information(this,
                             tr("Information"),
                             tr("Export completed successfully."),
                             QMessageBox::Ok);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onUpdateStatus()
{
    LOG(INFO) << "[ACTION] Start online checker";

    AddressBookTab* tab = currentAddressBookTab();
    if (!tab)
    {
        LOG(ERROR) << "No active tab";
        return;
    }

    tab->startOnlineChecker();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onOnlineHelp()
{
    LOG(INFO) << "[ACTION] Online help";

    if (!QDesktopServices::openUrl(QUrl("https://aspia.org/help")))
    {
        LOG(ERROR) << "Unable to open URL";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCheckUpdates()
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "[ACTION] Check for updates";
    common::UpdateDialog(Settings().updateServer(),"console", this).exec();
#endif
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAbout()
{
    LOG(INFO) << "[ACTION] About";
    common::AboutDialog(tr("Aspia Console"), this).exec();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onFastConnect()
{
    LOG(INFO) << "[ACTION] Fast Connect";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        FastConnectDialog(this, tab->addressBookGuid(), tab->rootComputerGroup()->config(), tab->routerConfig()).exec();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onDesktopManageConnect()
{
    LOG(INFO) << "[ACTION] Connect to desktop manage session";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer computer = computer_item->computerToConnect();
            computer.set_session_type(proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
            connectToComputer(computer, tab->displayName(), tab->routerConfig());
        }
        else
        {
            LOG(ERROR) << "No active computer";
        }
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onDesktopViewConnect()
{
    LOG(INFO) << "[ACTION] Connect to desktop view session";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer computer = computer_item->computerToConnect();
            computer.set_session_type(proto::peer::SESSION_TYPE_DESKTOP_VIEW);
            connectToComputer(computer, tab->displayName(), tab->routerConfig());
        }
        else
        {
            LOG(ERROR) << "No active computer";
        }
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onFileTransferConnect()
{
    LOG(INFO) << "[ACTION] Connect to file transfer session";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer computer = computer_item->computerToConnect();
            computer.set_session_type(proto::peer::SESSION_TYPE_FILE_TRANSFER);
            connectToComputer(computer, tab->displayName(), tab->routerConfig());
        }
        else
        {
            LOG(ERROR) << "No active computer";
        }
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onSystemInfoConnect()
{
    LOG(INFO) << "[ACTION] Connect to system info session";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer computer = computer_item->computerToConnect();
            computer.set_session_type(proto::peer::SESSION_TYPE_SYSTEM_INFO);
            connectToComputer(computer, tab->displayName(), tab->routerConfig());
        }
        else
        {
            LOG(ERROR) << "No active computer";
        }
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTextChatConnect()
{
    LOG(INFO) << "[ACTION] Connect to text chat session";

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer computer = computer_item->computerToConnect();
            computer.set_session_type(proto::peer::SESSION_TYPE_TEXT_CHAT);
            connectToComputer(computer, tab->displayName(), tab->routerConfig());
        }
        else
        {
            LOG(ERROR) << "No active computer";
        }
    }
    else
    {
        LOG(ERROR) << "No active tab";
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCurrentTabChanged(int index)
{
    LOG(INFO) << "[ACTION] Current tab changed to:" << index;

    if (index == -1)
    {
        ui.action_save->setEnabled(false);
        ui.action_add_computer_group->setEnabled(false);
        ui.action_add_computer->setEnabled(false);
        ui.action_fast_connect->setEnabled(false);
        ui.action_router_manage->setEnabled(false);
        ui.action_update_status->setEnabled(false);
        ui.action_import_computers->setEnabled(false);
        ui.action_export_computers->setEnabled(false);
        return;
    }

    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
        if (tab)
        {
            if (index != i)
                tab->stopOnlineChecker();

            if (tab->isChanged())
                ui.action_save_all->setEnabled(true);
        }
    }

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
    {
        LOG(INFO) << "No active tab";
        return;
    }

    ui.action_save->setEnabled(tab->isChanged());
    ui.action_close->setEnabled(!mru_.isPinnedFile(tab->filePath()));

    ui.action_fast_connect->setEnabled(true);
    ui.action_router_manage->setEnabled(tab->isRouterEnabled());

    proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
    if (computer_group)
        ui.status_bar->setCurrentComputerGroup(*computer_group);
    else
        ui.status_bar->clear();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onCloseTab(int index)
{
    LOG(INFO) << "[ACTION] Close tab:" << index;

    if (index == -1)
        return;

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
    {
        LOG(INFO) << "No active tab";
        return;
    }

    tab->stopOnlineChecker();

    if (mru_.isPinnedFile(tab->filePath()))
    {
        LOG(INFO) << "Tab not pinned";
        return;
    }

    if (tab->isChanged())
    {
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Address book \"%1\" has been changed. Save changes?")
                                    .arg(tab->addressBookName()),
                                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));
        message_box.button(QMessageBox::Cancel)->setText(tr("Cancel"));

        switch (message_box.exec())
        {
            case QMessageBox::Yes:
                tab->save();
                break;

            case QMessageBox::Cancel:
                return;

            default:
                break;
        }
    }

    ui.tab_widget->removeTab(index);
    delete tab;

    if (!ui.tab_widget->count())
    {
        ui.status_bar->clear();
        ui.action_save_as->setEnabled(false);
        ui.action_save_all->setEnabled(false);
        ui.action_address_book_properties->setEnabled(false);
    }

    if (!hasUnpinnedTabs())
    {
        ui.action_close->setEnabled(false);
        ui.action_close_all->setEnabled(false);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onAddressBookChanged(bool changed)
{
    ui.action_save->setEnabled(changed);

    if (changed)
    {
        ui.action_save_all->setEnabled(true);

        int current_tab_index = ui.tab_widget->currentIndex();
        if (current_tab_index == -1)
            return;

        AddressBookTab* tab =
            dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab_index));
        if (!tab)
            return;

        // Update tab title.
        ui.tab_widget->setTabText(current_tab_index, tab->addressBookName());
        ui.action_router_manage->setEnabled(tab->isRouterEnabled());
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onComputerGroupActivated(bool activated, bool is_root)
{
    ui.action_add_computer_group->setEnabled(activated);
    ui.action_add_computer->setEnabled(activated);
    ui.action_update_status->setEnabled(activated);

    ui.action_import_computers->setEnabled(activated);
    ui.action_export_computers->setEnabled(activated);

    ui.action_copy_computer->setEnabled(false);
    ui.action_modify_computer->setEnabled(false);
    ui.action_delete_computer->setEnabled(false);

    if (is_root)
    {
        ui.action_modify_computer_group->setEnabled(false);
        ui.action_delete_computer_group->setEnabled(false);
    }
    else
    {
        ui.action_modify_computer_group->setEnabled(activated);
        ui.action_delete_computer_group->setEnabled(activated);
    }

    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
        if (computer_group)
            ui.status_bar->setCurrentComputerGroup(*computer_group);
        else
            ui.status_bar->clear();
    }
    else
    {
        ui.status_bar->clear();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onComputerActivated(bool activated)
{
    ui.action_modify_computer->setEnabled(activated);
    ui.action_copy_computer->setEnabled(activated);
    ui.action_delete_computer->setEnabled(activated);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onComputerGroupContextMenu(const QPoint& point, bool is_root)
{
    QMenu menu;

    if (is_root)
    {
        menu.addAction(ui.action_address_book_properties);
    }
    else
    {
        menu.addAction(ui.action_modify_computer_group);
        menu.addAction(ui.action_delete_computer_group);
    }

    menu.addSeparator();
    menu.addAction(ui.action_add_computer_group);
    menu.addAction(ui.action_add_computer);
    menu.addSeparator();
    menu.addAction(ui.action_import_computers);
    menu.addAction(ui.action_export_computers);

    menu.exec(point);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onComputerContextMenu(ComputerItem* computer_item, const QPoint& point)
{
    QMenu menu;

    if (computer_item)
    {
        menu.addAction(ui.action_desktop_manage_connect);
        menu.addAction(ui.action_desktop_view_connect);
        menu.addAction(ui.action_file_transfer_connect);
        menu.addAction(ui.action_text_chat_connect);
        menu.addAction(ui.action_system_info_connect);
        menu.addSeparator();
        menu.addAction(ui.action_modify_computer);
        menu.addAction(ui.action_copy_computer);
        menu.addAction(ui.action_delete_computer);
    }
    else
    {
        menu.addAction(ui.action_add_computer);
    }

    menu.exec(point);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onComputerDoubleClicked(const proto::address_book::Computer& computer)
{
    AddressBookTab* tab = currentAddressBookTab();
    if (!tab)
    {
        LOG(INFO) << "No active tab";
        return;
    }

    ComputerItem* computer_item = tab->currentComputer();
    if (!computer_item)
    {
        LOG(INFO) << "No active computer";
        return;
    }

    proto::address_book::Computer computer_to_connect(computer);

    if (ui.action_desktop_manage->isChecked())
    {
        computer_to_connect.set_session_type(proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
    }
    else if (ui.action_desktop_view->isChecked())
    {
        computer_to_connect.set_session_type(proto::peer::SESSION_TYPE_DESKTOP_VIEW);
    }
    else if (ui.action_file_transfer->isChecked())
    {
        computer_to_connect.set_session_type(proto::peer::SESSION_TYPE_FILE_TRANSFER);
    }
    else if (ui.action_system_info->isChecked())
    {
        computer_to_connect.set_session_type(proto::peer::SESSION_TYPE_SYSTEM_INFO);
    }
    else if (ui.action_text_chat->isChecked())
    {
        computer_to_connect.set_session_type(proto::peer::SESSION_TYPE_TEXT_CHAT);
    }
    else
    {
        LOG(FATAL) << "Unknown session type";
        return;
    }

    connectToComputer(computer_to_connect, tab->displayName(), tab->routerConfig());
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onTabContextMenu(const QPoint& pos)
{
    QTabBar* tab_bar = ui.tab_widget->tabBar();
    int tab_index = tab_bar->tabAt(pos);
    if (tab_index == -1)
        return;

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(tab_index));
    if (!tab)
        return;

    const QString& current_path = tab->filePath();
    bool is_pinned = mru_.isPinnedFile(current_path);

    QMenu menu;

    QAction* close_other_action = nullptr;
    QAction* close_action;
    QAction* pin_action;

    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab_at = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (!tab_at)
            continue;

        if (i != tab_index && !mru_.isPinnedFile(tab_at->filePath()))
        {
            close_other_action = new QAction(tr("Close other tabs"), &menu);
            break;
        }
    }

    if (!is_pinned)
    {
        close_action = new QAction(tr("Close tab"), &menu);
        pin_action = new QAction(tr("Pin tab"), &menu);
    }
    else
    {
        close_action = nullptr;
        pin_action = new QAction(tr("Pin tab"), &menu);
    }

    pin_action->setCheckable(true);
    pin_action->setChecked(is_pinned);

    if (close_action)
        menu.addAction(close_action);
    if (close_other_action)
        menu.addAction(close_other_action);

    menu.addAction(pin_action);

    QAction* action = menu.exec(tab_bar->mapToGlobal(pos));
    if (!action)
        return;

    if (action == close_action)
    {
        onCloseTab(tab_index);
    }
    else if (action == close_other_action)
    {
        for (int i = ui.tab_widget->count(); i >= 0; --i)
        {
            if (i != tab_index)
                onCloseTab(i);
        }
    }
    else if (action == pin_action)
    {
        if (pin_action->isChecked())
        {
            if (tab->isChanged())
            {
                if (!tab->save())
                    return;
            }

            ui.tab_widget->setTabIcon(tab_index, QIcon(":/img/pin-file.svg"));
            mru_.pinFile(current_path);
        }
        else
        {
            ui.tab_widget->setTabIcon(tab_index, QIcon(":/img/file.svg"));
            mru_.unpinFile(current_path);
        }

        QWidget* close_button = ui.tab_widget->tabBar()->tabButton(tab_index, QTabBar::RightSide);
        if (!close_button)
            close_button = ui.tab_widget->tabBar()->tabButton(tab_index, QTabBar::LeftSide);

        if (close_button)
        {
            close_button->setVisible(!pin_action->isChecked());
        }
        else
        {
            LOG(ERROR) << "No close button";
        }

        bool has_unpinned_tabs = hasUnpinnedTabs();
        ui.action_close->setEnabled(has_unpinned_tabs);
        ui.action_close_all->setEnabled(has_unpinned_tabs);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onLanguageChanged(QAction* action)
{
    Application* application = Application::instance();

    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();

    Settings().setLocale(new_locale);
    application->setLocale(new_locale);

    ui.retranslateUi(this);

    for (int i = 0; i < ui.tab_widget->count(); ++i)
        static_cast<AddressBookTab*>(ui.tab_widget->widget(i))->retranslateUi();

    rebuildMruMenu();
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onRecentOpenTriggered(QAction* action)
{
    if (action == ui.action_clear_mru ||
        (action == ui.action_remember_last && !action->isChecked()))
    {
        QMessageBox message_box(
            QMessageBox::Question,
            tr("Confirmation"),
            tr("The list of recently opened address books will be cleared. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        if (message_box.exec() == QMessageBox::Yes)
        {
            mru_.clearRecentOpen();
            rebuildMruMenu();
        }

        Settings().setRecentOpenEnabled(action->isChecked());
    }
    else if (action == ui.action_remember_last)
    {
        Settings().setRecentOpenEnabled(true);
    }
    else
    {
        MruAction* mru_action = dynamic_cast<MruAction*>(action);
        if (!mru_action)
            return;

        QString file_path = mru_action->filePath();
        if (file_path.isEmpty())
            return;

        openAddressBook(file_path);
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::onShowHideToTray()
{
    if (isHidden())
    {
        ui.action_show_hide->setText(tr("Hide"));

        if (!ui.action_show_tray_icon->isChecked())
            showTrayIcon(false);

        if (windowState() & Qt::WindowMaximized)
            showMaximized();
        else
            showNormal();

        activateWindow();
        setFocus();
    }
    else
    {
        ui.action_show_hide->setText(tr("Show"));

        if (!ui.action_show_tray_icon->isChecked())
            showTrayIcon(true);

        hide();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && ui.action_minimize_to_tray->isChecked())
    {
        if (windowState() & Qt::WindowMinimized)
            onShowHideToTray();
    }

    QMainWindow::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event detected";

    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
        {
            QMessageBox message_box(QMessageBox::Question,
                                    tr("Confirmation"),
                                    tr("Address book \"%1\" has been changed. Save changes?")
                                        .arg(tab->addressBookName()),
                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                    this);
            message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
            message_box.button(QMessageBox::No)->setText(tr("No"));
            message_box.button(QMessageBox::Cancel)->setText(tr("Cancel"));

            switch (message_box.exec())
            {
                case QMessageBox::Yes:
                    tab->save();
                    break;

                case QMessageBox::Cancel:
                    event->ignore();
                    return;

                default:
                    break;
            }
        }
    }

    Settings settings;

    settings.setToolBarEnabled(ui.action_toolbar->isChecked());
    settings.setStatusBarEnabled(ui.action_statusbar->isChecked());
    settings.setAlwaysShowTrayIcon(ui.action_show_tray_icon->isChecked());
    settings.setMinimizeToTray(ui.action_minimize_to_tray->isChecked());
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());

    settings.setRecentOpenEnabled(ui.action_remember_last->isChecked());
    settings.setRecentOpen(mru_.recentOpen());
    settings.setPinnedFiles(mru_.pinnedFiles());

    if (ui.action_desktop_manage->isChecked())
        settings.setSessionType(proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
    else if (ui.action_desktop_view->isChecked())
        settings.setSessionType(proto::peer::SESSION_TYPE_DESKTOP_VIEW);
    else if (ui.action_file_transfer->isChecked())
        settings.setSessionType(proto::peer::SESSION_TYPE_FILE_TRANSFER);
    else if (ui.action_system_info->isChecked())
        settings.setSessionType(proto::peer::SESSION_TYPE_SYSTEM_INFO);
    else if (ui.action_text_chat->isChecked())
        settings.setSessionType(proto::peer::SESSION_TYPE_TEXT_CHAT);

    QApplication::quit();
    QMainWindow::closeEvent(event);
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
        update_checker_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void MainWindow::createLanguageMenu(const QString& current_locale)
{
    Application::LocaleList locale_list = Application::instance()->localeList();
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
void MainWindow::rebuildMruMenu()
{
    QList<QAction*> actions = ui.menu_recent_open->actions();
    for (QAction* action : std::as_const(actions))
    {
        MruAction* mru_action = dynamic_cast<MruAction*>(action);
        if (mru_action)
            ui.menu_recent_open->removeAction(action);
    }

    const QStringList file_list = mru_.recentOpen();

    if (file_list.isEmpty())
    {
        ui.menu_recent_open->addAction(new MruAction(QString(), ui.menu_recent_open));
    }
    else
    {
        for (const auto& file : file_list)
            ui.menu_recent_open->addAction(new MruAction(file, ui.menu_recent_open));
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::showTrayIcon(bool show)
{
    if (show)
    {
        tray_menu_.reset(new QMenu(this));
        tray_menu_->addAction(ui.action_show_hide);
        tray_menu_->addAction(ui.action_exit);

        tray_icon_.reset(new QSystemTrayIcon(this));
        tray_icon_->setIcon(QIcon(":/img/aspia-console.ico"));
        tray_icon_->setToolTip(tr("Aspia Console"));
        tray_icon_->setContextMenu(tray_menu_.get());
        tray_icon_->show();

        connect(tray_icon_.get(), &QSystemTrayIcon::activated,
                this, [this](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::Context)
                return;

            onShowHideToTray();
        });
    }
    else
    {
        tray_icon_.reset();
        tray_menu_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void MainWindow::addAddressBookTab(AddressBookTab* new_tab)
{
    if (!new_tab)
        return;

    const QString& file_path = new_tab->filePath();

    if (Settings().isRecentOpenEnabled())
    {
        if (mru_.addRecentFile(file_path))
            rebuildMruMenu();
    }

    connect(new_tab, &AddressBookTab::sig_addressBookChanged,
            this, &MainWindow::onAddressBookChanged);
    connect(new_tab, &AddressBookTab::sig_computerGroupActivated,
            this, &MainWindow::onComputerGroupActivated);
    connect(new_tab, &AddressBookTab::sig_computerActivated,
            this, &MainWindow::onComputerActivated);
    connect(new_tab, &AddressBookTab::sig_computerGroupContextMenu,
            this, &MainWindow::onComputerGroupContextMenu);
    connect(new_tab, &AddressBookTab::sig_computerContextMenu,
            this, &MainWindow::onComputerContextMenu);
    connect(new_tab, &AddressBookTab::sig_computerDoubleClicked,
            this, &MainWindow::onComputerDoubleClicked);
    connect(new_tab, &AddressBookTab::sig_updateStateForComputers,
            ui.status_bar, &StatusBar::setUpdateState);

    QIcon icon = QIcon(mru_.isPinnedFile(file_path) ? ":/img/pin-file.svg" : ":/img/file.svg");
    int index = ui.tab_widget->addTab(new_tab, icon, new_tab->addressBookName());

    QWidget* close_button = ui.tab_widget->tabBar()->tabButton(index, QTabBar::RightSide);
    if (!close_button)
        close_button = ui.tab_widget->tabBar()->tabButton(index, QTabBar::LeftSide);

    if (close_button)
    {
        if (mru_.isPinnedFile(file_path))
            close_button->hide();
        else
            close_button->show();
    }
    else
    {
        LOG(ERROR) << "No close button";
    }

    bool has_unpinned_tabs = hasUnpinnedTabs();
    ui.action_close->setEnabled(has_unpinned_tabs);
    ui.action_close_all->setEnabled(has_unpinned_tabs);

    ui.action_address_book_properties->setEnabled(true);
    ui.action_save_as->setEnabled(true);

    ui.tab_widget->setCurrentIndex(index);
}

//--------------------------------------------------------------------------------------------------
AddressBookTab* MainWindow::currentAddressBookTab()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab == -1)
        return nullptr;

    return dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
}

//--------------------------------------------------------------------------------------------------
bool MainWindow::hasChangedTabs() const
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool MainWindow::hasUnpinnedTabs() const
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && !mru_.isPinnedFile(tab->filePath()))
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void MainWindow::connectToComputer(const proto::address_book::Computer& computer,
                                   const QString& display_name,
                                   const std::optional<client::RouterConfig>& router_config)
{
    QString address = QString::fromStdString(computer.address());
    bool host_id_entered = true;

    for (int i = 0; i < address.length(); ++i)
    {
        if (!address[i].isDigit())
        {
            host_id_entered = false;
            break;
        }
    }

    if (host_id_entered && !router_config.has_value())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Connection by ID is specified in the properties of the computer, "
                                "but the router is not configured. Check the parameters of the "
                                "router in the properties of the address book."),
                             QMessageBox::Ok);
        return;
    }

    client::Config config;
    config.router_config = router_config;
    config.computer_name = QString::fromStdString(computer.name());
    config.address_or_id = QString::fromStdString(computer.address());
    config.port          = static_cast<quint16>(computer.port());
    config.username      = QString::fromStdString(computer.username());
    config.password      = QString::fromStdString(computer.password());
    config.session_type  = computer.session_type();
    config.display_name  = display_name;

    client::SessionWindow* session_window = nullptr;

    switch (config.session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
        {
            session_window = new client::DesktopSessionWindow(
                config.session_type, computer.session_config().desktop_manage());
        }
        break;

        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
        {
            session_window = new client::DesktopSessionWindow(
                config.session_type, computer.session_config().desktop_view());
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            session_window = new client::FileTransferSessionWindow();
            break;

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            session_window = new client::SystemInfoSessionWindow();
            break;

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            session_window = new client::TextChatSessionWindow();
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
void MainWindow::connectToRouter()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (!tab)
    {
        LOG(ERROR) << "No active tab";
        return;
    }

    std::optional<client::RouterConfig> router_config = tab->routerConfig();
    if (!router_config.has_value())
    {
        LOG(ERROR) << "No config for router";
        return;
    }

    client::RouterManagerWindow* client_window = new client::RouterManagerWindow();
    client_window->setAttribute(Qt::WA_DeleteOnClose);
    client_window->connectToRouter(*router_config);
}

} // namespace console
