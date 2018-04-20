//
// PROJECT:         Aspia
// FILE:            console/console_window.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__CONSOLE_WINDOW_H
#define _ASPIA_CONSOLE__CONSOLE_WINDOW_H

#include "console/address_book.h"
#include "ui_console_window.h"

namespace aspia {

class ConsoleWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConsoleWindow(const QString& file_path, QWidget* parent = nullptr);
    ~ConsoleWindow();

public slots:
    void newAction();
    void openAction();
    void saveAction();
    void saveAsAction();
    void closeAction();
    void addressBookPropertiesAction();
    void addComputerAction();
    void modifyComputerAction();
    void deleteComputerAction();
    void addComputerGroupAction();
    void modifyComputerGroupAction();
    void deleteComputerGroupAction();
    void onlineHelpAction();
    void aboutAction();
    void exitAction();
    void groupItemClicked(QTreeWidgetItem* item, int column);
    void groupContextMenu(const QPoint& point);
    void groupItemCollapsed(QTreeWidgetItem* item);
    void groupItemExpanded(QTreeWidgetItem* item);
    void computerItemClicked(QTreeWidgetItem* item, int column);
    void computerContextMenu(const QPoint& point);
    void desktopManageSessionToggled(bool checked);
    void desktopViewSessionToggled(bool checked);
    void fileTransferSessionToggled(bool checked);

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) override;

private:
    void showOpenError(const QString& message);
    void showSaveError(const QString& message);
    void updateComputerList(ComputerGroup* computer_group);
    void setChanged(bool changed);
    bool openAddressBook(const QString& file_path);
    bool saveAddressBook(const QString& file_path);
    bool closeAddressBook();

    Ui::ConsoleWindow ui;

    QString file_path_;
    QString password_;
    bool is_changed_ = false;

    proto::AddressBook::EncryptionType encryption_type_ =
        proto::AddressBook::ENCRYPTION_TYPE_NONE;

    std::unique_ptr<AddressBook> address_book_;

    Q_DISABLE_COPY(ConsoleWindow)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__CONSOLE_WINDOW_H
