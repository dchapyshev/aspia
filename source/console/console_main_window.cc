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

#include "console/console_main_window.h"

#include "base/logging.h"
#include "base/strings/unicode.h"
#include "build/build_config.h"
#include "build/version.h"
#include "client/ui/client_dialog.h"
#include "client/ui/qt_desktop_window.h"
#include "client/ui/qt_file_manager_window.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "console/address_book_tab.h"
#include "console/console_application.h"
#include "console/mru_action.h"
#include "console/update_settings_dialog.h"
#include "updater/update_dialog.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>

namespace console {

MainWindow::MainWindow(const QString& file_path)
{
    Settings& settings = Application::instance()->settings();

    ui.setupUi(this);

    createLanguageMenu(settings.locale());

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
    connect(ui.action_modify_computer, &QAction::triggered, this, &MainWindow::onModifyComputer);

    connect(ui.action_delete_computer, &QAction::triggered,
            this, &MainWindow::onDeleteComputer);

    connect(ui.action_add_computer_group, &QAction::triggered,
            this, &MainWindow::onAddComputerGroup);

    connect(ui.action_modify_computer_group, &QAction::triggered,
            this, &MainWindow::onModifyComputerGroup);

    connect(ui.action_delete_computer_group, &QAction::triggered,
            this, &MainWindow::onDeleteComputerGroup);

    connect(ui.action_online_help, &QAction::triggered, this, &MainWindow::onOnlineHelp);
    connect(ui.action_check_updates, &QAction::triggered, this, &MainWindow::onCheckUpdates);
    connect(ui.action_about, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &MainWindow::close);
    connect(ui.action_fast_connect, &QAction::triggered, this, &MainWindow::onFastConnect);

    connect(ui.action_desktop_manage_connect, &QAction::triggered,
            this, &MainWindow::onDesktopManageConnect);

    connect(ui.action_desktop_view_connect, &QAction::triggered,
            this, &MainWindow::onDesktopViewConnect);

    connect(ui.action_file_transfer_connect, &QAction::triggered,
            this, &MainWindow::onFileTransferConnect);

    connect(ui.tool_bar, &QToolBar::visibilityChanged, ui.action_toolbar, &QAction::setChecked);
    connect(ui.action_toolbar, &QAction::toggled, ui.tool_bar, &QToolBar::setVisible);
    connect(ui.action_statusbar, &QAction::toggled, ui.status_bar, &QStatusBar::setVisible);
    connect(ui.tab_widget, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(ui.tab_widget, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);
    connect(ui.menu_language, &QMenu::triggered, this, &MainWindow::onLanguageChanged);

    QActionGroup* session_type_group = new QActionGroup(this);

    session_type_group->addAction(ui.action_desktop_manage);
    session_type_group->addAction(ui.action_desktop_view);
    session_type_group->addAction(ui.action_file_transfer);

    switch (settings.sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            ui.action_desktop_manage->setChecked(true);
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            ui.action_desktop_view->setChecked(true);
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            ui.action_file_transfer->setChecked(true);
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

    connect(ui.action_update_settings, &QAction::triggered, [this]()
    {
        UpdateSettingsDialog(this).exec();
    });

    if (settings.checkUpdates())
    {
        updater::Checker* checker = new updater::Checker(this);

        checker->setUpdateServer(settings.updateServer());
        checker->setPackageName(QStringLiteral("console"));

        connect(checker, &updater::Checker::finished, this, &MainWindow::onUpdateChecked);
        connect(checker, &updater::Checker::finished, checker, &updater::Checker::deleteLater);

        checker->start();
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::showConsole()
{
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

void MainWindow::openAddressBook(const QString& file_path)
{
    showConsole();

    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab)
        {
#if defined(OS_WIN)
            if (file_path.compare(tab->filePath(), Qt::CaseInsensitive) == 0)
#else
            if (file_path.compare(tab->addressBookPath(), Qt::CaseSensitive) == 0)
#endif // defined(OS_WIN)
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

void MainWindow::onNew()
{
    addAddressBookTab(AddressBookTab::createNew(ui.tab_widget));
}

void MainWindow::onOpen()
{
    Settings& settings = Application::instance()->settings();

    QString file_path =
        QFileDialog::getOpenFileName(this,
                                     tr("Open Address Book"),
                                     settings.lastDirectory(),
                                     tr("Aspia Address Book (*.aab)"));
    if (file_path.isEmpty())
        return;

    settings.setLastDirectory(QFileInfo(file_path).absolutePath());
    openAddressBook(file_path);
}

void MainWindow::onSave()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab && tab->save())
    {
        if (Application::instance()->settings().isRecentOpenEnabled())
        {
            if (mru_.addRecentFile(tab->filePath()))
                rebuildMruMenu();
        }

        if (!hasChangedTabs())
            ui.action_save_all->setEnabled(false);
    }
}

void MainWindow::onSaveAs()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        QString old_path = tab->filePath();
        if (mru_.isPinnedFile(old_path))
        {
            QScopedPointer<AddressBookTab> duplicate_tab(tab->duplicateTab());
            if (duplicate_tab->saveAs())
            {
                if (Application::instance()->settings().isRecentOpenEnabled())
                {
                    const QString& new_path = duplicate_tab->filePath();
                    if (mru_.addRecentFile(new_path))
                        rebuildMruMenu();
                }

                addAddressBookTab(duplicate_tab.take());
            }
        }
        else if (tab->saveAs())
        {
            if (Application::instance()->settings().isRecentOpenEnabled())
            {
                const QString& new_path = tab->filePath();
                if (mru_.addRecentFile(new_path))
                    rebuildMruMenu();
            }
        }

        if (!hasChangedTabs())
            ui.action_save_all->setEnabled(false);
    }
}

void MainWindow::onSaveAll()
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
            tab->save();
    }

    if (!hasChangedTabs())
        ui.action_save_all->setEnabled(false);
}

void MainWindow::onClose()
{
    onCloseTab(ui.tab_widget->currentIndex());
}

void MainWindow::onCloseAll()
{
    for (int i = ui.tab_widget->count(); i >= 0; --i)
        onCloseTab(i);
}

void MainWindow::onAddressBookProperties()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyAddressBook();
}

void MainWindow::onAddComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->addComputer();
}

void MainWindow::onModifyComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyComputer();
}

void MainWindow::onDeleteComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->removeComputer();
}

void MainWindow::onAddComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->addComputerGroup();
}

void MainWindow::onModifyComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyComputerGroup();
}

void MainWindow::onDeleteComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->removeComputerGroup();
}

void MainWindow::onOnlineHelp()
{
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

void MainWindow::onCheckUpdates()
{
    updater::UpdateDialog(Application::instance()->settings().updateServer(),
                          QLatin1String("console"),
                          this).exec();
}

void MainWindow::onAbout()
{
    common::AboutDialog(this).exec();
}

void MainWindow::onFastConnect()
{
    client::ClientDialog().exec();
}

void MainWindow::onDesktopManageConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer* computer = computer_item->computer();
            computer->set_session_type(proto::SESSION_TYPE_DESKTOP_MANAGE);
            connectToComputer(*computer);
        }
    }
}

void MainWindow::onDesktopViewConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer* computer = computer_item->computer();
            computer->set_session_type(proto::SESSION_TYPE_DESKTOP_VIEW);
            connectToComputer(*computer);
        }
    }
}

void MainWindow::onFileTransferConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        ComputerItem* computer_item = tab->currentComputer();
        if (computer_item)
        {
            proto::address_book::Computer* computer = computer_item->computer();
            computer->set_session_type(proto::SESSION_TYPE_FILE_TRANSFER);
            connectToComputer(*computer);
        }
    }
}

void MainWindow::onCurrentTabChanged(int index)
{
    if (index == -1)
    {
        ui.action_save->setEnabled(false);
        ui.action_add_computer_group->setEnabled(false);
        ui.action_add_computer->setEnabled(false);
        return;
    }

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
        return;

    ui.action_save->setEnabled(tab->isChanged());
    ui.action_close->setEnabled(!mru_.isPinnedFile(tab->filePath()));

    proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
    if (computer_group)
        ui.status_bar->setCurrentComputerGroup(*computer_group);
    else
        ui.status_bar->clear();
}

void MainWindow::onCloseTab(int index)
{
    if (index == -1)
        return;

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
        return;

    if (mru_.isPinnedFile(tab->filePath()))
        return;

    if (tab->isChanged())
    {
        int ret = QMessageBox(QMessageBox::Question,
                              tr("Confirmation"),
                              tr("Address book \"%1\" has been changed. Save changes?")
                              .arg(tab->addressBookName()),
                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                              this).exec();
        switch (ret)
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
    }
}

void MainWindow::onComputerGroupActivated(bool activated, bool is_root)
{
    ui.action_add_computer_group->setEnabled(activated);

    ui.action_add_computer->setEnabled(activated);
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

void MainWindow::onComputerActivated(bool activated)
{
    ui.action_modify_computer->setEnabled(activated);
    ui.action_delete_computer->setEnabled(activated);
}

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

    menu.exec(point);
}

void MainWindow::onComputerContextMenu(ComputerItem* computer_item, const QPoint& point)
{
    QMenu menu;

    if (computer_item)
    {
        menu.addAction(ui.action_desktop_manage_connect);
        menu.addAction(ui.action_desktop_view_connect);
        menu.addAction(ui.action_file_transfer_connect);
        menu.addSeparator();
        menu.addAction(ui.action_modify_computer);
        menu.addAction(ui.action_delete_computer);
    }
    else
    {
        menu.addAction(ui.action_add_computer);
    }

    menu.exec(point);
}

void MainWindow::onComputerDoubleClicked(proto::address_book::Computer* computer)
{
    AddressBookTab* tab = currentAddressBookTab();
    if (!tab)
        return;

    ComputerItem* computer_item = tab->currentComputer();
    if (!computer_item)
        return;

    if (ui.action_desktop_manage->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_DESKTOP_MANAGE);
    }
    else if (ui.action_desktop_view->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_DESKTOP_VIEW);
    }
    else if (ui.action_file_transfer->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_FILE_TRANSFER);
    }
    else
    {
        LOG(LS_FATAL) << "Unknown session type";
        return;
    }

    connectToComputer(*computer);
}

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
            close_other_action = new QAction(
                QIcon(QStringLiteral(":/img/ui-tab-multi-close.png")), tr("Close other tabs"), &menu);
            break;
        }
    }

    if (!is_pinned)
    {
        close_action = new QAction(
            QIcon(QStringLiteral(":/img/ui-tab-close.png")), tr("Close tab"), &menu);

        pin_action = new QAction(
            QIcon(QStringLiteral(":/img/lock-unlock.png")), tr("Pin tab"), &menu);
    }
    else
    {
        close_action = nullptr;

        pin_action = new QAction(
            QIcon(QStringLiteral(":/img/lock.png")), tr("Pin tab"), &menu);
    }

    pin_action->setCheckable(true);
    pin_action->setChecked(is_pinned);

    if (close_action)
        menu.addAction(close_action);
    if (close_other_action)
        menu.addAction(close_other_action);

    if (close_action || close_other_action)
        menu.addSeparator();

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

            ui.tab_widget->setTabIcon(
                tab_index, QIcon(QStringLiteral(":/img/address-book-pinned.png")));
            mru_.pinFile(current_path);
        }
        else
        {
            ui.tab_widget->setTabIcon(
                tab_index, QIcon(QStringLiteral(":/img/address-book.png")));
            mru_.unpinFile(current_path);
        }

        QWidget* close_button = ui.tab_widget->tabBar()->tabButton(tab_index, QTabBar::RightSide);
        close_button->setVisible(!pin_action->isChecked());

        bool has_unpinned_tabs = hasUnpinnedTabs();
        ui.action_close->setEnabled(has_unpinned_tabs);
        ui.action_close_all->setEnabled(has_unpinned_tabs);
    }
}

void MainWindow::onLanguageChanged(QAction* action)
{
    Application* application = Application::instance();

    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();

    application->settings().setLocale(new_locale);
    application->setLocale(new_locale);

    ui.retranslateUi(this);

    for (int i = 0; i < ui.tab_widget->count(); ++i)
        static_cast<AddressBookTab*>(ui.tab_widget->widget(i))->retranslateUi();

    rebuildMruMenu();
}

void MainWindow::onRecentOpenTriggered(QAction* action)
{
    if (action == ui.action_clear_mru ||
        (action == ui.action_remember_last && !action->isChecked()))
    {
        int ret = QMessageBox(
            QMessageBox::Question,
            tr("Confirmation"),
            tr("The list of recently opened address books will be cleared. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            this).exec();
        if (ret == QMessageBox::Yes)
        {
            mru_.clearRecentOpen();
            rebuildMruMenu();
        }

        Application::instance()->settings().setRecentOpenEnabled(action->isChecked());
    }
    else if (action == ui.action_remember_last)
    {
        Application::instance()->settings().setRecentOpenEnabled(true);
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

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && ui.action_minimize_to_tray->isChecked())
    {
        if (windowState() & Qt::WindowMinimized)
            onShowHideToTray();
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
        {
            int ret = QMessageBox(QMessageBox::Question,
                                  tr("Confirmation"),
                                  tr("Address book \"%1\" has been changed. Save changes?")
                                  .arg(tab->addressBookName()),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  this).exec();
            switch (ret)
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

    Settings& settings = Application::instance()->settings();

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
        settings.setSessionType(proto::SESSION_TYPE_DESKTOP_MANAGE);
    else if (ui.action_desktop_view->isChecked())
        settings.setSessionType(proto::SESSION_TYPE_DESKTOP_VIEW);
    else if (ui.action_file_transfer->isChecked())
        settings.setSessionType(proto::SESSION_TYPE_FILE_TRANSFER);

    QApplication::quit();
    QMainWindow::closeEvent(event);
}

void MainWindow::onUpdateChecked(const updater::UpdateInfo& update_info)
{
    if (!update_info.isValid() || !update_info.hasUpdate())
        return;

    base::Version current_version(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);

    if (update_info.version() > current_version)
        updater::UpdateDialog(update_info, this).exec();
}

void MainWindow::createLanguageMenu(const QString& current_locale)
{
    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : Application::instance()->localeList())
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

void MainWindow::rebuildMruMenu()
{
    for (QAction* action : ui.menu_recent_open->actions())
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

void MainWindow::showTrayIcon(bool show)
{
    if (show)
    {
        tray_menu_.reset(new QMenu(this));
        tray_menu_->addAction(ui.action_show_hide);
        tray_menu_->addAction(ui.action_exit);

        tray_icon_.reset(new QSystemTrayIcon(this));
        tray_icon_->setIcon(QIcon(QStringLiteral(":/img/main.png")));
        tray_icon_->setToolTip(tr("Aspia Console"));
        tray_icon_->setContextMenu(tray_menu_.get());
        tray_icon_->show();

        connect(tray_icon_.get(), &QSystemTrayIcon::activated,
                [this](QSystemTrayIcon::ActivationReason reason)
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

void MainWindow::addAddressBookTab(AddressBookTab* new_tab)
{
    if (!new_tab)
        return;

    const QString& file_path = new_tab->filePath();

    if (Application::instance()->settings().isRecentOpenEnabled())
    {
        if (mru_.addRecentFile(file_path))
            rebuildMruMenu();
    }

    connect(new_tab, &AddressBookTab::addressBookChanged,
            this, &MainWindow::onAddressBookChanged);
    connect(new_tab, &AddressBookTab::computerGroupActivated,
            this, &MainWindow::onComputerGroupActivated);
    connect(new_tab, &AddressBookTab::computerActivated,
            this, &MainWindow::onComputerActivated);
    connect(new_tab, &AddressBookTab::computerGroupContextMenu,
            this, &MainWindow::onComputerGroupContextMenu);
    connect(new_tab, &AddressBookTab::computerContextMenu,
            this, &MainWindow::onComputerContextMenu);
    connect(new_tab, &AddressBookTab::computerDoubleClicked,
            this, &MainWindow::onComputerDoubleClicked);

    QIcon icon = mru_.isPinnedFile(file_path) ?
        QIcon(QStringLiteral(":/img/address-book-pinned.png")) :
        QIcon(QStringLiteral(":/img/address-book.png"));

    int index = ui.tab_widget->addTab(new_tab, icon, new_tab->addressBookName());

    QWidget* close_button = ui.tab_widget->tabBar()->tabButton(index, QTabBar::RightSide);
    if (mru_.isPinnedFile(file_path))
        close_button->hide();
    else
        close_button->show();

    bool has_unpinned_tabs = hasUnpinnedTabs();
    ui.action_close->setEnabled(has_unpinned_tabs);
    ui.action_close_all->setEnabled(has_unpinned_tabs);

    ui.action_address_book_properties->setEnabled(true);
    ui.action_save_as->setEnabled(true);

    ui.tab_widget->setCurrentIndex(index);
}

AddressBookTab* MainWindow::currentAddressBookTab()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab == -1)
        return nullptr;

    return dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
}

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

void MainWindow::connectToComputer(const proto::address_book::Computer& computer)
{
    client::Config config;

    config.computer_name = base::utf16FromUtf8(computer.name());
    config.address       = base::utf16FromUtf8(computer.address());
    config.port          = computer.port();
    config.username      = base::utf16FromUtf8(computer.username());
    config.password      = base::utf16FromUtf8(computer.password());
    config.session_type  = computer.session_type();

    client::ClientWindow* client_window = nullptr;

    switch (config.session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        {
            client_window = new client::QtDesktopWindow(
                config.session_type, computer.session_config().desktop_manage());
        }
        break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            client_window = new client::QtDesktopWindow(
                config.session_type, computer.session_config().desktop_view());
        }
        break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            client_window = new client::QtFileManagerWindow();
            break;

        default:
            NOTREACHED();
            break;
    }

    if (!client_window)
        return;

    client_window->setAttribute(Qt::WA_DeleteOnClose);
    if (!client_window->connectToHost(config))
        client_window->close();
}

} // namespace console
