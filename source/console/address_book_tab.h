//
// SmartCafe Project
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

#ifndef CONSOLE_ADDRESS_BOOK_TAB_H
#define CONSOLE_ADDRESS_BOOK_TAB_H

#include <optional>
#include <memory>

#include "client/router_config.h"
#include "client/online_checker/online_checker.h"
#include "proto/address_book.h"
#include "ui_address_book_tab.h"

namespace console {

class ComputerItem;

class AddressBookTab final : public QWidget
{
    Q_OBJECT

public:
    ~AddressBookTab() final;

    static AddressBookTab* createNew(QWidget* parent);
    static AddressBookTab* openFromFile(const QString& file_path, QWidget* parent);

    QString addressBookName() const;
    QString addressBookGuid() const;
    const QString& filePath() const { return file_path_; }
    ComputerItem* currentComputer() const;
    QString displayName() const;
    proto::address_book::ComputerGroup* currentComputerGroup() const;
    proto::address_book::ComputerGroup* rootComputerGroup();
    void setChanged(bool changed);
    bool isChanged() const { return is_changed_; }

    AddressBookTab* duplicateTab() const;

    bool save();
    bool saveAs();
    void reloadAll();

    bool isRouterEnabled() const;
    std::optional<client::RouterConfig> routerConfig() const;

    void retranslateUi();

public slots:
    void addComputerGroup();
    void addComputer();
    void copyComputer();
    void modifyAddressBook();
    void modifyComputerGroup();
    void modifyComputer();
    void removeComputerGroup();
    void removeComputer();
    void startOnlineChecker();
    void stopOnlineChecker();

signals:
    void sig_addressBookChanged(bool changed);
    void sig_computerGroupActivated(bool activated, bool is_root);
    void sig_computerActivated(bool activated);
    void sig_computerGroupContextMenu(const QPoint& point, bool is_root);
    void sig_computerContextMenu(console::ComputerItem* comouter_item, const QPoint& point);
    void sig_computerDoubleClicked(const proto::address_book::Computer& computer);
    void sig_updateStateForComputers(bool started);

protected:
    // ConsoleTab implementation.
    void showEvent(QShowEvent* event) final;
    void keyPressEvent(QKeyEvent* event) final;

private slots:
    void onOnlineCheckerResult(int computer_id, bool online);
    void onOnlineCheckerFinished();
    void onGroupItemClicked(QTreeWidgetItem* item, int column);
    void onGroupContextMenu(const QPoint& point);
    void onGroupItemCollapsed(QTreeWidgetItem* item);
    void onGroupItemExpanded(QTreeWidgetItem* item);
    void onGroupItemDropped();
    void onComputerItemClicked(QTreeWidgetItem* item, int column);
    void onComputerContextMenu(const QPoint& point);
    void onComputerItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    AddressBookTab(const QString& file_path,
                   proto::address_book::File&& file,
                   proto::address_book::Data&& data,
                   const QByteArray& key,
                   QWidget* parent);

    QByteArray saveState();
    void restoreState(const QByteArray& state);
    void updateComputerList(ComputerGroupItem* computer_group);
    bool saveToFile(const QString& file_path);
    ComputerGroupItem* rootComputerGroupItem();

    static QString parentName(ComputerGroupItem* item);
    static void showOpenError(QWidget* parent, const QString& message);
    static void showSaveError(QWidget* parent, const QString& message);

    Ui::AddressBookTab ui;

    QString file_path_;
    QByteArray key_;

    proto::address_book::File file_;
    proto::address_book::Data data_;

    bool is_changed_ = false;

    std::unique_ptr<client::OnlineChecker> online_checker_;

    Q_DISABLE_COPY(AddressBookTab)
};

} // namespace console

#endif // CONSOLE_ADDRESS_BOOK_TAB_H
