//
// PROJECT:         Aspia
// FILE:            console/console_window.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__CONSOLE_WINDOW_H
#define _ASPIA_CONSOLE__CONSOLE_WINDOW_H

#include "protocol/computer.pb.h"
#include "ui_console_window.h"

namespace aspia {

class AddressBookTab;

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
    void fastConnectAction();
    void desktopManageSessionConnect();
    void desktopViewSessionConnect();
    void fileTransferSessionConnect();
    void desktopManageSessionToggled(bool checked);
    void desktopViewSessionToggled(bool checked);
    void fileTransferSessionToggled(bool checked);

    void currentTabChanged(int index);
    void closeTab(int index);

    void onAddressBookChanged(bool changed);
    void onComputerGroupActivated(bool activated, bool is_root);
    void onComputerActivated(bool activated);
    void onComputerGroupContextMenu(const QPoint& point, bool is_root);
    void onComputerContextMenu(const QPoint& point);
    void onComputerDoubleClicked(proto::Computer* computer);

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) override;

private:
    void addAddressBookTab(AddressBookTab* tab);
    void connectToComputer(const proto::Computer& computer);

    Ui::ConsoleWindow ui;

    Q_DISABLE_COPY(ConsoleWindow)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__CONSOLE_WINDOW_H
