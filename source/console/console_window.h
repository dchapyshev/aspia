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

#include <QSystemTrayIcon>

#include "base/locale_loader.h"
#include "client/client_connections.h"
#include "protocol/address_book.pb.h"
#include "ui_console_window.h"

namespace aspia {

class AddressBookTab;
class ComputerItem;
class Client;

class ConsoleWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConsoleWindow(const QString& file_path, QWidget* parent = nullptr);
    ~ConsoleWindow() = default;

public slots:
    void onNewAddressBook();
    void onOpenAddressBook();
    void onSaveAddressBook();
    void onSaveAsAddressBook();
    void onCloseAddressBook();
    void onAddressBookProperties();
    void onAddComputer();
    void onModifyComputer();
    void onDeleteComputer();
    void onAddComputerGroup();
    void onModifyComputerGroup();
    void onDeleteComputerGroup();
    void onOnlineHelp();
    void onAbout();
    void onFastConnect();
    void onDesktopManageConnect();
    void onDesktopViewConnect();
    void onFileTransferConnect();
    void onSystemInfoConnect();

    void onCurrentTabChanged(int index);
    void onCloseTab(int index);

    void onAddressBookChanged(bool changed);
    void onComputerGroupActivated(bool activated, bool is_root);
    void onComputerActivated(bool activated);
    void onComputerGroupContextMenu(const QPoint& point, bool is_root);
    void onComputerContextMenu(ComputerItem* computer_item, const QPoint& point);
    void onComputerDoubleClicked(proto::address_book::Computer* computer);
    void onLanguageChanged(QAction* action);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowHideToTray();

protected:
    // QMainWindow implementation.
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void createLanguageMenu(const QString& current_locale);
    void addAddressBookTab(AddressBookTab* tab);
    AddressBookTab* currentAddressBookTab();
    void connectToComputer(const proto::address_book::Computer& computer);

    Ui::ConsoleWindow ui;

    LocaleLoader locale_loader_;
    ClientConnections connections_;

    QSystemTrayIcon tray_icon_;
    QMenu tray_menu_;

    DISALLOW_COPY_AND_ASSIGN(ConsoleWindow);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__CONSOLE_WINDOW_H_
