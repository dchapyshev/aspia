//
// PROJECT:         Aspia
// FILE:            console/address_book_tab.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H
#define _ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H

#include <QWidget>

#include "console/console_tab.h"
#include "protocol/address_book.pb.h"
#include "ui_address_book_tab.h"

namespace aspia {

class AddressBookTab : public ConsoleTab
{
    Q_OBJECT

public:
    ~AddressBookTab();

    static AddressBookTab* createNew(QWidget* parent);
    static AddressBookTab* openFromFile(const QString& file_path, QWidget* parent);

    QString addressBookName() const;
    QString addressBookPath() const;
    proto::address_book::Computer* currentComputer() const;
    void setChanged(bool changed);
    bool isChanged() const { return is_changed_; }

public slots:
    void save();
    void saveAs();
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
    void computerContextMenu(const QPoint& point);
    void computerDoubleClicked(proto::address_book::Computer* computer);

protected:
    // ConsoleTab implementation.
    void showEvent(QShowEvent* event) override;

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

    Ui::AddressBookTab ui;

    QString file_path_;
    QByteArray key_;

    proto::address_book::File file_;
    proto::address_book::Data data_;

    bool is_changed_ = false;

    Q_DISABLE_COPY(AddressBookTab)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H
