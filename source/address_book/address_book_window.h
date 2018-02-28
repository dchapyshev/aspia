//
// PROJECT:         Aspia
// FILE:            address_book/address_book_window.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__ADDRESS_BOOK_WINDOW_H
#define _ASPIA_ADDRESS_BOOK__ADDRESS_BOOK_WINDOW_H

#include <QtWidgets/QMainWindow>

#include "address_book/address_book.h"
#include "qt_generated/ui_address_book_window.h"

namespace aspia {

class AddressBookWindow : public QMainWindow
{
    Q_OBJECT

public:
    AddressBookWindow(const QString& file_path, QWidget* parent = nullptr);
    ~AddressBookWindow() = default;

public slots:
    void OnNewAction();
    void OnOpenAction();
    void OnSaveAction();
    void OnSaveAsAction();
    void OnCloseAction();
    void OnAddressBookPropertiesAction();
    void OnAddComputerAction();
    void OnModifyComputerAction();
    void OnDeleteComputerAction();
    void OnAddComputerGroupAction();
    void OnModifyComputerGroupAction();
    void OnDeleteComputerGroupAction();
    void OnOnlineHelpAction();
    void OnExitAction();
    void OnGroupItemClicked(QTreeWidgetItem* item, int column);
    void OnGroupContextMenu(const QPoint& point);
    void OnGroupItemCollapsed(QTreeWidgetItem* item);
    void OnGroupItemExpanded(QTreeWidgetItem* item);
    void OnComputerItemClicked(QTreeWidgetItem* item, int column);
    void OnComputerContextMenu(const QPoint& point);
    void OnDesktopManageSessionToggled(bool checked);
    void OnDesktopViewSessionToggled(bool checked);
    void OnFileTransferSessionToggled(bool checked);
    void OnSystemInfoSessionToggled(bool checked);

private:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) override;

    void ShowOpenError(const QString& message);
    void ShowSaveError(const QString& message);
    void UpdateComputerList(ComputerGroup* computer_group);
    void SetChanged(bool changed);
    bool OpenAddressBook(const QString& file_path);
    bool SaveAddressBook(const QString& file_path);
    bool CloseAddressBook();

    Ui::AddressBookWindow ui;

    QString file_path_;
    QString password_;
    bool is_changed_ = false;

    proto::AddressBook::EncryptionType encryption_type_ =
        proto::AddressBook::ENCRYPTION_TYPE_NONE;

    std::unique_ptr<AddressBook> address_book_;

    DISALLOW_COPY_AND_ASSIGN(AddressBookWindow);
};

} // namespace aspia

#endif // _ASPIA_ADDRESS_BOOK__ADDRESS_BOOK_WINDOW_H
