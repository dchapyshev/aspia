//
// PROJECT:         Aspia
// FILE:            console/console_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_window.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>

#include "client/client.h"
#include "client/ui/client_dialog.h"
#include "console/about_dialog.h"
#include "console/address_book_tab.h"
#include "console/console_settings.h"

namespace aspia {

ConsoleWindow::ConsoleWindow(const QString& file_path, QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    ConsoleSettings settings;
    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    connect(ui.action_new, &QAction::triggered, this, &ConsoleWindow::onNewAddressBook);
    connect(ui.action_open, &QAction::triggered, this, &ConsoleWindow::onOpenAddressBook);
    connect(ui.action_save, &QAction::triggered, this, &ConsoleWindow::onSaveAddressBook);
    connect(ui.action_save_as, &QAction::triggered, this, &ConsoleWindow::onSaveAsAddressBook);
    connect(ui.action_close, &QAction::triggered, this, &ConsoleWindow::onCloseAddressBook);

    connect(ui.action_address_book_properties, &QAction::triggered,
            this, &ConsoleWindow::onAddressBookProperties);

    connect(ui.action_add_computer, &QAction::triggered, this, &ConsoleWindow::onAddComputer);
    connect(ui.action_modify_computer, &QAction::triggered, this, &ConsoleWindow::onModifyComputer);

    connect(ui.action_delete_computer, &QAction::triggered,
            this, &ConsoleWindow::onDeleteComputer);

    connect(ui.action_add_computer_group, &QAction::triggered,
            this, &ConsoleWindow::onAddComputerGroup);

    connect(ui.action_modify_computer_group, &QAction::triggered,
            this, &ConsoleWindow::onModifyComputerGroup);

    connect(ui.action_delete_computer_group, &QAction::triggered,
            this, &ConsoleWindow::onDeleteComputerGroup);

    connect(ui.action_online_help, &QAction::triggered, this, &ConsoleWindow::onOnlineHelp);
    connect(ui.action_about, &QAction::triggered, this, &ConsoleWindow::onAbout);
    connect(ui.action_exit, &QAction::triggered, this, &ConsoleWindow::onExit);
    connect(ui.action_fast_connect, &QAction::triggered, this, &ConsoleWindow::onFastConnect);

    connect(ui.action_desktop_manage_connect, &QAction::triggered,
            this, &ConsoleWindow::onDesktopManageConnect);

    connect(ui.action_desktop_view_connect, &QAction::triggered,
            this, &ConsoleWindow::onDesktopViewConnect);

    connect(ui.action_file_transfer_connect, &QAction::triggered,
            this, &ConsoleWindow::onFileTransferConnect);

    connect(ui.action_desktop_manage, &QAction::toggled,
            this, &ConsoleWindow::onDesktopManageToggled);

    connect(ui.action_desktop_view, &QAction::toggled,
            this, &ConsoleWindow::onDesktopViewToggled);

    connect(ui.action_file_transfer, &QAction::toggled,
            this, &ConsoleWindow::onFileTransferToggled);

    connect(ui.tab_widget, &QTabWidget::currentChanged, this, &ConsoleWindow::onCurrentTabChanged);
    connect(ui.tab_widget, &QTabWidget::tabCloseRequested, this, &ConsoleWindow::onCloseTab);

    if (!file_path.isEmpty())
        addAddressBookTab(AddressBookTab::openFromFile(file_path, ui.tab_widget));
}

ConsoleWindow::~ConsoleWindow() = default;

void ConsoleWindow::onNewAddressBook()
{
    addAddressBookTab(AddressBookTab::createNew(ui.tab_widget));
}

void ConsoleWindow::onOpenAddressBook()
{
    ConsoleSettings settings;

    QString file_path =
        QFileDialog::getOpenFileName(this,
                                     tr("Open Address Book"),
                                     settings.lastDirectory(),
                                     tr("Aspia Address Book (*.aab)"));
    if (file_path.isEmpty())
        return;

    settings.setLastDirectory(QFileInfo(file_path).absolutePath());

    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab)
        {
#if defined(Q_OS_WIN)
            if (file_path.compare(tab->addressBookPath(), Qt::CaseInsensitive) == 0)
#else
            if (file_path.compare(tab->addressBookPath(), Qt::CaseSensitive) == 0)
#endif // defined(Q_OS_WIN)
            {
                QMessageBox::information(this,
                                         tr("Information"),
                                         tr("Address Book \"%1\" is already open.").arg(file_path),
                                         QMessageBox::Ok);

                ui.tab_widget->setCurrentIndex(i);
                return;
            }
        }
    }

    addAddressBookTab(AddressBookTab::openFromFile(file_path, ui.tab_widget));
}

void ConsoleWindow::onSaveAddressBook()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->save();
    }
}

void ConsoleWindow::onSaveAsAddressBook()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->saveAs();
    }
}

void ConsoleWindow::onCloseAddressBook()
{
    onCloseTab(ui.tab_widget->currentIndex());
}

void ConsoleWindow::onAddressBookProperties()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->modifyAddressBook();
    }
}

void ConsoleWindow::onAddComputer()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->addComputer();
    }
}

void ConsoleWindow::onModifyComputer()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->modifyComputer();
    }
}

void ConsoleWindow::onDeleteComputer()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->removeComputer();
    }
}

void ConsoleWindow::onAddComputerGroup()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->addComputerGroup();
    }
}

void ConsoleWindow::onModifyComputerGroup()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->modifyComputerGroup();
    }
}

void ConsoleWindow::onDeleteComputerGroup()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->removeComputerGroup();
    }
}

void ConsoleWindow::onOnlineHelp()
{
    QDesktopServices::openUrl(QUrl(tr("https://aspia.net/help")));
}

void ConsoleWindow::onAbout()
{
    AboutDialog(this).exec();
}

void ConsoleWindow::onExit()
{
    close();
}

void ConsoleWindow::onFastConnect()
{
    ClientDialog dialog(this);

    if (dialog.exec() != ClientDialog::Accepted)
        return;

    connectToComputer(&dialog.computer());
}

void ConsoleWindow::onDesktopManageConnect()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
        {
            proto::address_book::Computer* computer = tab->currentComputer();
            if (computer)
            {
                computer->set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
                connectToComputer(computer);
            }
        }
    }
}

void ConsoleWindow::onDesktopViewConnect()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
        {
            proto::address_book::Computer* computer = tab->currentComputer();
            if (computer)
            {
                computer->set_session_type(proto::auth::SESSION_TYPE_DESKTOP_VIEW);
                connectToComputer(computer);
            }
        }
    }
}

void ConsoleWindow::onFileTransferConnect()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
        {
            proto::address_book::Computer* computer = tab->currentComputer();
            if (computer)
            {
                computer->set_session_type(proto::auth::SESSION_TYPE_FILE_TRANSFER);
                connectToComputer(computer);
            }
        }
    }
}

void ConsoleWindow::onDesktopManageToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_view->setChecked(false);
        ui.action_file_transfer->setChecked(false);
    }
}

void ConsoleWindow::onDesktopViewToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_file_transfer->setChecked(false);
    }
}

void ConsoleWindow::onFileTransferToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_desktop_view->setChecked(false);
    }
}

void ConsoleWindow::onCurrentTabChanged(int index)
{
    if (index == -1)
    {
        ui.action_save->setEnabled(false);
        ui.action_add_computer_group->setEnabled(false);
        ui.action_add_computer->setEnabled(false);
        return;
    }

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
        return;

    ui.action_save->setEnabled(tab->isChanged());
}

void ConsoleWindow::onCloseTab(int index)
{
    if (index == -1)
        return;

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(index));
    if (!tab)
        return;

    if (tab->isChanged())
    {
        int ret = QMessageBox(QMessageBox::Question,
                              tr("Confirmation"),
                              tr("Address book \"%1\" has been changed. Save changes?")
                              .arg(tab->addressBookName()),
                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                              this).exec();
        switch (ret)
        {
            case QMessageBox::Yes:
                tab->save();
                break;

            case QMessageBox::Cancel:
                return;

            default:
                break;
        }
    }

    ui.tab_widget->removeTab(index);
    delete tab;

    if (!ui.tab_widget->count())
    {
        ui.action_save_as->setEnabled(false);
        ui.action_address_book_properties->setEnabled(false);
        ui.action_close->setEnabled(false);
    }
}

void ConsoleWindow::onAddressBookChanged(bool changed)
{
    ui.action_save->setEnabled(changed);

    if (changed)
    {
        int current_tab_index = ui.tab_widget->currentIndex();
        if (current_tab_index == -1)
            return;

        AddressBookTab* tab =
            dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab_index));
        if (!tab)
            return;

        // Update tab title.
        ui.tab_widget->setTabText(current_tab_index, tab->addressBookName());
    }
}

void ConsoleWindow::onComputerGroupActivated(bool activated, bool is_root)
{
    ui.action_add_computer_group->setEnabled(activated);

    ui.action_add_computer->setEnabled(activated);
    ui.action_modify_computer->setEnabled(false);
    ui.action_delete_computer->setEnabled(false);

    if (is_root)
    {
        ui.action_modify_computer_group->setEnabled(false);
        ui.action_delete_computer_group->setEnabled(false);
    }
    else
    {
        ui.action_modify_computer_group->setEnabled(activated);
        ui.action_delete_computer_group->setEnabled(activated);
    }
}

void ConsoleWindow::onComputerActivated(bool activated)
{
    ui.action_modify_computer->setEnabled(activated);
    ui.action_delete_computer->setEnabled(activated);
}

void ConsoleWindow::onComputerGroupContextMenu(const QPoint& point, bool is_root)
{
    QMenu menu;

    if (is_root)
    {
        menu.addAction(ui.action_address_book_properties);
    }
    else
    {
        menu.addAction(ui.action_modify_computer_group);
        menu.addAction(ui.action_delete_computer_group);
    }

    menu.addSeparator();
    menu.addAction(ui.action_add_computer_group);
    menu.addAction(ui.action_add_computer);

    menu.exec(point);
}

void ConsoleWindow::onComputerContextMenu(const QPoint& point)
{
    QMenu menu;

    menu.addAction(ui.action_desktop_manage_connect);
    menu.addAction(ui.action_desktop_view_connect);
    menu.addAction(ui.action_file_transfer_connect);
    menu.addSeparator();
    menu.addAction(ui.action_modify_computer);
    menu.addAction(ui.action_delete_computer);

    menu.exec(point);
}

void ConsoleWindow::onComputerDoubleClicked(proto::address_book::Computer* computer)
{
    if (ui.action_desktop_manage->isChecked())
    {
        computer->set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
    }
    else if (ui.action_desktop_view->isChecked())
    {
        computer->set_session_type(proto::auth::SESSION_TYPE_DESKTOP_VIEW);
    }
    else if (ui.action_file_transfer->isChecked())
    {
        computer->set_session_type(proto::auth::SESSION_TYPE_FILE_TRANSFER);
    }
    else
    {
        qFatal("Unknown session type");
        return;
    }

    connectToComputer(computer);
}

void ConsoleWindow::closeEvent(QCloseEvent* event)
{
    for (int i = 0; i < ui.tab_widget->count(); ++i)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
        if (tab && tab->isChanged())
        {
            int ret = QMessageBox(QMessageBox::Question,
                                  tr("Confirmation"),
                                  tr("Address book \"%1\" has been changed. Save changes?")
                                  .arg(tab->addressBookName()),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  this).exec();
            switch (ret)
            {
                case QMessageBox::Yes:
                    tab->save();
                    break;

                case QMessageBox::Cancel:
                    return;

                default:
                    break;
            }
        }
    }

    ConsoleSettings settings;
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());

    QApplication::quit();
    QMainWindow::closeEvent(event);
}

void ConsoleWindow::addAddressBookTab(AddressBookTab* new_tab)
{
    if (!new_tab)
        return;

    connect(new_tab, &AddressBookTab::addressBookChanged,
            this, &ConsoleWindow::onAddressBookChanged);
    connect(new_tab, &AddressBookTab::computerGroupActivated,
            this, &ConsoleWindow::onComputerGroupActivated);
    connect(new_tab, &AddressBookTab::computerActivated,
            this, &ConsoleWindow::onComputerActivated);
    connect(new_tab, &AddressBookTab::computerGroupContextMenu,
            this, &ConsoleWindow::onComputerGroupContextMenu);
    connect(new_tab, &AddressBookTab::computerContextMenu,
            this, &ConsoleWindow::onComputerContextMenu);
    connect(new_tab, &AddressBookTab::computerDoubleClicked,
            this, &ConsoleWindow::onComputerDoubleClicked);

    int index = ui.tab_widget->addTab(new_tab,
                                      QIcon(QStringLiteral(":/icon/address-book.png")),
                                      new_tab->addressBookName());

    ui.action_address_book_properties->setEnabled(true);
    ui.action_save_as->setEnabled(true);
    ui.action_close->setEnabled(true);

    ui.tab_widget->setCurrentIndex(index);
}

void ConsoleWindow::connectToComputer(proto::address_book::Computer* computer)
{
    computer->set_connect_time(QDateTime::currentSecsSinceEpoch());

    Client* client = new Client(*computer, this);
    connect(client, &Client::clientTerminated, client, &Client::deleteLater);
}

} // namespace aspia
