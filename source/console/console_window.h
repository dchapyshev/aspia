//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CONSOLE__CONSOLE_WINDOW_H_
#define ASPIA_CONSOLE__CONSOLE_WINDOW_H_

#include "common/locale_loader.h"
#include "console/mru.h"
#include "updater/update_checker.h"
#include "protocol/address_book.pb.h"
#include "ui_console_window.h"

class QSystemTrayIcon;

namespace aspia {

class AddressBookTab;
class ComputerItem;
class Client;

class ConsoleWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConsoleWindow(LocaleLoader& locale_loader, const QString& file_path);
    ~ConsoleWindow();

public slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onSaveAll();
    void onClose();
    void onCloseAll();
    void onAddressBookProperties();
    void onAddComputer();
    void onModifyComputer();
    void onDeleteComputer();
    void onAddComputerGroup();
    void onModifyComputerGroup();
    void onDeleteComputerGroup();
    void onOnlineHelp();
    void onCheckUpdates();
    void onAbout();
    void onFastConnect();
    void onDesktopManageConnect();
    void onDesktopViewConnect();
    void onFileTransferConnect();

    void onCurrentTabChanged(int index);
    void onCloseTab(int index);

    void onAddressBookChanged(bool changed);
    void onComputerGroupActivated(bool activated, bool is_root);
    void onComputerActivated(bool activated);
    void onComputerGroupContextMenu(const QPoint& point, bool is_root);
    void onComputerContextMenu(ComputerItem* computer_item, const QPoint& point);
    void onComputerDoubleClicked(proto::address_book::Computer* computer);
    void onTabContextMenu(const QPoint& pos);
    void onLanguageChanged(QAction* action);
    void onShowHideToTray();

protected:
    // QMainWindow implementation.
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onUpdateChecked(const UpdateInfo& update_info);

private:
    void createLanguageMenu(const QString& current_locale);
    void rebuildMruMenu();
    void showTrayIcon(bool show);
    void openAddressBook(const QString& file_path);
    void addAddressBookTab(AddressBookTab* tab);
    AddressBookTab* currentAddressBookTab();
    bool hasChangedTabs() const;
    bool hasUnpinnedTabs() const;
    void connectToComputer(const proto::address_book::Computer& computer);

    Ui::ConsoleWindow ui;

    LocaleLoader& locale_loader_;
    Mru mru_;

    QScopedPointer<QSystemTrayIcon> tray_icon_;
    QScopedPointer<QMenu> tray_menu_;

    DISALLOW_COPY_AND_ASSIGN(ConsoleWindow);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__CONSOLE_WINDOW_H_
