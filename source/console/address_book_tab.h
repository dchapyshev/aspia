//
// PROJECT:         Aspia
// FILE:            console/address_book_tab.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H
#define _ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H

#include <QWidget>

#include "protocol/address_book.pb.h"
#include "ui_address_book_tab.h"

namespace aspia {

class AddressBookTab : public QWidget
{
    Q_OBJECT

public:
    ~AddressBookTab();

    static AddressBookTab* createNewAddressBook(QWidget* parent = nullptr);
    static AddressBookTab* openAddressBook(const QString& file_path, QWidget* parent = nullptr);

    QString addressBookName() const;
    QString addressBookPath() const;
    proto::Computer* currentComputer() const;
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
    void computerDoubleClicked(proto::Computer* computer);

protected:
    // QWidget implementation.
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
                   proto::address_book::EncryptionType encryption_type,
                   const QString& password,
                   proto::address_book::ComputerGroup root_group,
                   QWidget* parent);

    void updateComputerList(ComputerGroupItem* computer_group);
    bool saveToFile(const QString& file_path);

    Ui::AddressBookTab ui;

    QString file_path_;
    QString password_;
    bool is_changed_ = false;

    proto::address_book::EncryptionType encryption_type_ =
        proto::address_book::ENCRYPTION_TYPE_NONE;

    proto::address_book::ComputerGroup root_group_;

    Q_DISABLE_COPY(AddressBookTab)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__ADDRESS_BOOK_TAB_H
