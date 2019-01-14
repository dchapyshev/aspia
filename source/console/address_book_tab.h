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

#ifndef ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H
#define ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H

#include "base/macros_magic.h"
#include "console/console_tab.h"
#include "proto/address_book.pb.h"
#include "ui_address_book_tab.h"

namespace console {

class ComputerItem;

class AddressBookTab : public ConsoleTab
{
    Q_OBJECT

public:
    ~AddressBookTab();

    static AddressBookTab* createNew(QWidget* parent);
    static AddressBookTab* openFromFile(const QString& file_path, QWidget* parent);

    QString addressBookName() const;
    const QString& filePath() const { return file_path_; }
    ComputerItem* currentComputer() const;
    proto::address_book::ComputerGroup* currentComputerGroup() const;
    void setChanged(bool changed);
    bool isChanged() const { return is_changed_; }

    AddressBookTab* duplicateTab() const;

    bool save();
    bool saveAs();

    void retranslateUi();

public slots:
    void addComputerGroup();
    void addComputer();
    void modifyAddressBook();
    void modifyComputerGroup();
    void modifyComputer();
    void removeComputerGroup();
    void removeComputer();

signals:
    void addressBookChanged(bool changed);
    void computerGroupActivated(bool activated, bool is_root);
    void computerActivated(bool activated);
    void computerGroupContextMenu(const QPoint& point, bool is_root);
    void computerContextMenu(ComputerItem* comouter_item, const QPoint& point);
    void computerDoubleClicked(proto::address_book::Computer* computer);

protected:
    // ConsoleTab implementation.
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
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
                   QByteArray&& key,
                   QWidget* parent);

    void updateComputerList(ComputerGroupItem* computer_group);
    bool saveToFile(const QString& file_path);

    static void showOpenError(QWidget* parent, const QString& message);
    static void showSaveError(QWidget* parent, const QString& message);

    Ui::AddressBookTab ui;

    QString file_path_;
    QByteArray key_;

    proto::address_book::File file_;
    proto::address_book::Data data_;

    bool is_changed_ = false;

    DISALLOW_COPY_AND_ASSIGN(AddressBookTab);
};

} // namespace console

#endif // ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H
