//
// PROJECT:         Aspia
// FILE:            console/console_window.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__CONSOLE_WINDOW_H
#define _ASPIA_CONSOLE__CONSOLE_WINDOW_H

#include <QMainWindow>

#include "console/address_book.h"
#include "qt/ui_console_window.h"

namespace aspia {

class ConsoleWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConsoleWindow(const QString& file_path, QWidget* parent = nullptr);
    ~ConsoleWindow() = default;

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
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) override;

    void ShowOpenError(const QString& message);
    void ShowSaveError(const QString& message);
    void UpdateComputerList(ComputerGroup* computer_group);
    void SetChanged(bool changed);
    bool OpenAddressBook(const QString& file_path);
    bool SaveAddressBook(const QString& file_path);
    bool CloseAddressBook();

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
