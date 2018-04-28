//
// PROJECT:         Aspia
// FILE:            console/console_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_window.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include "client/client.h"
#include "client/ui/client_dialog.h"
#include "console/about_dialog.h"
#include "console/address_book_tab.h"

namespace aspia {

ConsoleWindow::ConsoleWindow(const QString& file_path, QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("WindowGeometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("WindowState")).toByteArray());

    connect(ui.action_new, &QAction::triggered, this, &ConsoleWindow::newAction);
    connect(ui.action_open, &QAction::triggered, this, &ConsoleWindow::openAction);
    connect(ui.action_save, &QAction::triggered, this, &ConsoleWindow::saveAction);
    connect(ui.action_save_as, &QAction::triggered, this, &ConsoleWindow::saveAsAction);
    connect(ui.action_close, &QAction::triggered, this, &ConsoleWindow::closeAction);

    connect(ui.action_address_book_properties, &QAction::triggered,
            this, &ConsoleWindow::addressBookPropertiesAction);

    connect(ui.action_add_computer, &QAction::triggered,
            this, &ConsoleWindow::addComputerAction);

    connect(ui.action_modify_computer, &QAction::triggered,
            this, &ConsoleWindow::modifyComputerAction);

    connect(ui.action_delete_computer, &QAction::triggered,
            this, &ConsoleWindow::deleteComputerAction);

    connect(ui.action_add_computer_group, &QAction::triggered,
            this, &ConsoleWindow::addComputerGroupAction);

    connect(ui.action_modify_computer_group, &QAction::triggered,
            this, &ConsoleWindow::modifyComputerGroupAction);

    connect(ui.action_delete_computer_group, &QAction::triggered,
            this, &ConsoleWindow::deleteComputerGroupAction);

    connect(ui.action_online_help, &QAction::triggered, this, &ConsoleWindow::onlineHelpAction);
    connect(ui.action_about, &QAction::triggered, this, &ConsoleWindow::aboutAction);
    connect(ui.action_exit, &QAction::triggered, this, &ConsoleWindow::exitAction);

    connect(ui.action_fast_connect, &QAction::triggered, this, &ConsoleWindow::fastConnectAction);

    connect(ui.action_desktop_manage_connect, &QAction::triggered,
            this, &ConsoleWindow::desktopManageSessionConnect);

    connect(ui.action_desktop_view_connect, &QAction::triggered,
            this, &ConsoleWindow::desktopViewSessionConnect);

    connect(ui.action_file_transfer_connect, &QAction::triggered,
            this, &ConsoleWindow::fileTransferSessionConnect);

    connect(ui.action_desktop_manage, &QAction::toggled,
            this, &ConsoleWindow::desktopManageSessionToggled);

    connect(ui.action_desktop_view, &QAction::toggled,
            this, &ConsoleWindow::desktopViewSessionToggled);

    connect(ui.action_file_transfer, &QAction::toggled,
            this, &ConsoleWindow::fileTransferSessionToggled);

    connect(ui.tab_widget, &QTabWidget::currentChanged, this, &ConsoleWindow::currentTabChanged);
    connect(ui.tab_widget, &QTabWidget::tabCloseRequested, this, &ConsoleWindow::closeTab);

    if (!file_path.isEmpty())
        addAddressBookTab(AddressBookTab::openAddressBook(file_path, ui.tab_widget));
}

ConsoleWindow::~ConsoleWindow() = default;

void ConsoleWindow::newAction()
{
    addAddressBookTab(AddressBookTab::createNewAddressBook(ui.tab_widget));
}

void ConsoleWindow::openAction()
{
    QSettings settings;

    QString directory_path =
        settings.value(QStringLiteral("LastDirectory"), QDir::homePath()).toString();

    QString file_path =
        QFileDialog::getOpenFileName(this,
                                     tr("Open Address Book"),
                                     directory_path,
                                     tr("Aspia Address Book (*.aab)"));
    if (file_path.isEmpty())
        return;

    settings.setValue(QStringLiteral("LastDirectory"), QFileInfo(file_path).absolutePath());

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

    addAddressBookTab(AddressBookTab::openAddressBook(file_path, ui.tab_widget));
}

void ConsoleWindow::saveAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->save();
    }
}

void ConsoleWindow::saveAsAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->saveAs();
    }
}

void ConsoleWindow::closeAction()
{
    closeTab(ui.tab_widget->currentIndex());
}

void ConsoleWindow::addressBookPropertiesAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->modifyAddressBook();
    }
}

void ConsoleWindow::addComputerAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->addComputer();
    }
}

void ConsoleWindow::modifyComputerAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->modifyComputer();
    }
}

void ConsoleWindow::deleteComputerAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->removeComputer();
    }
}

void ConsoleWindow::addComputerGroupAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->addComputerGroup();
    }
}

void ConsoleWindow::modifyComputerGroupAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->modifyComputerGroup();
    }
}

void ConsoleWindow::deleteComputerGroupAction()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab != -1)
    {
        AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
        if (tab)
            tab->removeComputerGroup();
    }
}

void ConsoleWindow::onlineHelpAction()
{
    QDesktopServices::openUrl(QUrl(tr("https://aspia.net/help")));
}

void ConsoleWindow::aboutAction()
{
    AboutDialog(this).exec();
}

void ConsoleWindow::exitAction()
{
    close();
}

void ConsoleWindow::fastConnectAction()
{
    ClientDialog dialog(this);

    if (dialog.exec() != ClientDialog::Accepted)
        return;

    connectToComputer(dialog.computer());
}

void ConsoleWindow::desktopManageSessionConnect()
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
                connectToComputer(*computer);
            }
        }
    }
}

void ConsoleWindow::desktopViewSessionConnect()
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
                connectToComputer(*computer);
            }
        }
    }
}

void ConsoleWindow::fileTransferSessionConnect()
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
                connectToComputer(*computer);
            }
        }
    }
}

void ConsoleWindow::desktopManageSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_view->setChecked(false);
        ui.action_file_transfer->setChecked(false);
    }
}

void ConsoleWindow::desktopViewSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_file_transfer->setChecked(false);
    }
}

void ConsoleWindow::fileTransferSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_desktop_view->setChecked(false);
    }
}

void ConsoleWindow::currentTabChanged(int index)
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

void ConsoleWindow::closeTab(int index)
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

    connectToComputer(*computer);
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

    QSettings settings;
    settings.setValue(QStringLiteral("WindowGeometry"), saveGeometry());
    settings.setValue(QStringLiteral("WindowState"), saveState());

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

void ConsoleWindow::connectToComputer(const proto::address_book::Computer& computer)
{
    Client* client = new Client(computer, this);
    connect(client, &Client::clientTerminated, client, &Client::deleteLater);
}

} // namespace aspia
