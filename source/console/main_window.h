//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE_MAIN_WINDOW_H
#define CONSOLE_MAIN_WINDOW_H

#include "client/router_config.h"
#include "common/update_checker.h"
#include "console/mru.h"
#include "proto/address_book.pb.h"
#include "ui_main_window.h"

#include <optional>

class QSystemTrayIcon;

namespace console {

class AddressBookTab;
class ComputerItem;
class Client;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& file_path);
    ~MainWindow() final;

public slots:
    void showConsole();
    void openAddressBook(const QString& file_path);

protected:
    // QMainWindow implementation.
    void changeEvent(QEvent* event) final;
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onUpdateCheckedFinished(const QByteArray& result);
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onSaveAll();
    void onClose();
    void onCloseAll();
    void onAddressBookProperties();
    void onAddComputer();
    void onCopyComputer();
    void onModifyComputer();
    void onDeleteComputer();
    void onAddComputerGroup();
    void onModifyComputerGroup();
    void onDeleteComputerGroup();
    void onImportComputers();
    void onExportComputers();
    void onUpdateStatus();
    void onOnlineHelp();
    void onCheckUpdates();
    void onAbout();
    void onFastConnect();
    void onDesktopManageConnect();
    void onDesktopViewConnect();
    void onFileTransferConnect();
    void onSystemInfoConnect();
    void onTextChatConnect();
    void onPortForwardingConnect();

    void onCurrentTabChanged(int index);
    void onCloseTab(int index);

    void onAddressBookChanged(bool changed);
    void onComputerGroupActivated(bool activated, bool is_root);
    void onComputerActivated(bool activated);
    void onComputerGroupContextMenu(const QPoint& point, bool is_root);
    void onComputerContextMenu(console::ComputerItem* computer_item, const QPoint& point);
    void onComputerDoubleClicked(const proto::address_book::Computer& computer);
    void onTabContextMenu(const QPoint& pos);
    void onLanguageChanged(QAction* action);
    void onRecentOpenTriggered(QAction* action);
    void onShowHideToTray();

private:
    void createLanguageMenu(const QString& current_locale);
    void rebuildMruMenu();
    void showTrayIcon(bool show);
    void addAddressBookTab(AddressBookTab* tab);
    AddressBookTab* currentAddressBookTab();
    bool hasChangedTabs() const;
    bool hasUnpinnedTabs() const;
    void connectToComputer(const proto::address_book::Computer& computer,
                           const QString& display_name,
                           const std::optional<client::RouterConfig>& router_config);
    void connectToRouter();

    Ui::ConsoleMainWindow ui;
    Mru mru_;

    std::unique_ptr<QSystemTrayIcon> tray_icon_;
    std::unique_ptr<QMenu> tray_menu_;
    std::unique_ptr<common::UpdateChecker> update_checker_;

    DISALLOW_COPY_AND_ASSIGN(MainWindow);
};

} // namespace console

#endif // CONSOLE_MAIN_WINDOW_H
