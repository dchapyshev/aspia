//
// PROJECT:         Aspia
// FILE:            console/console_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_window.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QTranslator>

#include "client/ui/client_dialog.h"
#include "client/client.h"
#include "client/computer_factory.h"
#include "console/about_dialog.h"
#include "console/address_book_tab.h"
#include "console/console_settings.h"

namespace aspia {

namespace {

class LanguageAction : public QAction
{
public:
    LanguageAction(const QString& locale, QObject* parent = nullptr)
        : QAction(parent),
          locale_(locale)
    {
        setText(QLocale::languageToString(QLocale(locale).language()));
    }

    ~LanguageAction() = default;

    QString locale() const { return locale_; }

private:
    QString locale_;
    Q_DISABLE_COPY(LanguageAction)
};

} // namespace

ConsoleWindow::ConsoleWindow(const QString& file_path, QWidget* parent)
    : QMainWindow(parent)
{
    ConsoleSettings settings;

    QString current_locale = settings.locale();

    if (!locale_loader_.contains(current_locale))
    {
        current_locale = ConsoleSettings::defaultLocale();
        settings.setLocale(current_locale);
    }

    locale_loader_.installTranslators(current_locale);
    ui.setupUi(this);
    createLanguageMenu(current_locale);

    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    ui.action_toolbar->setChecked(settings.isToolBarEnabled());

    bool show_status_bar = settings.isStatusBarEnabled();
    ui.action_statusbar->setChecked(show_status_bar);
    ui.status_bar->setVisible(show_status_bar);

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
    connect(ui.action_exit, &QAction::triggered, this, &ConsoleWindow::close);
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

    connect(ui.action_toolbar, &QAction::toggled, ui.tool_bar, &QToolBar::setVisible);
    connect(ui.action_statusbar, &QAction::toggled, ui.status_bar, &QStatusBar::setVisible);

    connect(ui.tab_widget, &QTabWidget::currentChanged, this, &ConsoleWindow::onCurrentTabChanged);
    connect(ui.tab_widget, &QTabWidget::tabCloseRequested, this, &ConsoleWindow::onCloseTab);

    connect(ui.menu_language, &QMenu::triggered, this, &ConsoleWindow::onLanguageChanged);

    switch (settings.sessionType())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            ui.action_desktop_manage->setChecked(true);
            break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            ui.action_desktop_view->setChecked(true);
            break;

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
            ui.action_file_transfer->setChecked(true);
            break;

        default:
            break;
    }

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
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

void ConsoleWindow::onAbout()
{
    AboutDialog(this).exec();
}

void ConsoleWindow::onFastConnect()
{
    ClientDialog dialog(this);

    if (dialog.exec() != ClientDialog::Accepted)
        return;

    connectToComputer(dialog.computer());
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
                computer->set_connect_time(QDateTime::currentSecsSinceEpoch());
                computer->set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);

                connectToComputer(*computer);
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
                computer->set_connect_time(QDateTime::currentSecsSinceEpoch());
                computer->set_session_type(proto::auth::SESSION_TYPE_DESKTOP_VIEW);

                connectToComputer(*computer);
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
                computer->set_connect_time(QDateTime::currentSecsSinceEpoch());
                computer->set_session_type(proto::auth::SESSION_TYPE_FILE_TRANSFER);

                connectToComputer(*computer);
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

    proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
    if (computer_group)
        ui.status_bar->setCurrentComputerGroup(*computer_group);
    else
        ui.status_bar->clear();
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
        ui.status_bar->clear();
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

    AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->currentWidget());
    if (tab)
    {
        proto::address_book::ComputerGroup* computer_group = tab->currentComputerGroup();
        if (computer_group)
            ui.status_bar->setCurrentComputerGroup(*computer_group);
        else
            ui.status_bar->clear();
    }
    else
    {
        ui.status_bar->clear();
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

void ConsoleWindow::onComputerContextMenu(ComputerItem* computer_item, const QPoint& point)
{
    QMenu menu;

    if (computer_item)
    {
        menu.addAction(ui.action_desktop_manage_connect);
        menu.addAction(ui.action_desktop_view_connect);
        menu.addAction(ui.action_file_transfer_connect);
        menu.addSeparator();
        menu.addAction(ui.action_modify_computer);
        menu.addAction(ui.action_delete_computer);
    }
    else
    {
        menu.addAction(ui.action_add_computer);
    }

    menu.exec(point);
}

void ConsoleWindow::onComputerDoubleClicked(proto::address_book::Computer* computer)
{
    computer->set_connect_time(QDateTime::currentSecsSinceEpoch());

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

void ConsoleWindow::onLanguageChanged(QAction* action)
{
    LanguageAction* language_action = dynamic_cast<LanguageAction*>(action);
    if (language_action)
    {
        QString new_locale = language_action->locale();

        locale_loader_.installTranslators(new_locale);
        ui.retranslateUi(this);

        for (int i = 0; i < ui.tab_widget->count(); ++i)
        {
            AddressBookTab* tab = dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(i));
            if (tab)
                tab->retranslateUi();
        }

        ConsoleSettings().setLocale(new_locale);
    }
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
    settings.setToolBarEnabled(ui.action_toolbar->isChecked());
    settings.setStatusBarEnabled(ui.action_statusbar->isChecked());
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());

    if (ui.action_desktop_manage->isChecked())
        settings.setSessionType(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
    else if (ui.action_desktop_view->isChecked())
        settings.setSessionType(proto::auth::SESSION_TYPE_DESKTOP_VIEW);
    else if (ui.action_file_transfer->isChecked())
        settings.setSessionType(proto::auth::SESSION_TYPE_FILE_TRANSFER);

    QApplication::quit();
    QMainWindow::closeEvent(event);
}

void ConsoleWindow::createLanguageMenu(const QString& current_locale)
{
    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : locale_loader_.sortedLocaleList())
    {
        LanguageAction* action_language = new LanguageAction(locale, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale)
            action_language->setChecked(true);

        ui.menu_language->addAction(action_language);
    }
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
    ConnectData connect_data;

    connect_data.setAddress(QString::fromStdString(computer.address()));
    connect_data.setPort(computer.port());
    connect_data.setUserName(QString::fromStdString(computer.username()));
    connect_data.setPassword(QString::fromStdString(computer.password()));
    connect_data.setSessionType(computer.session_type());

    switch (computer.session_type())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        {
            if (computer.session_config().has_desktop_manage())
                connect_data.setDesktopConfig(computer.session_config().desktop_manage());
            else
                connect_data.setDesktopConfig(ComputerFactory::defaultDesktopManageConfig());
        }
        break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        {
            if (computer.session_config().has_desktop_view())
                connect_data.setDesktopConfig(computer.session_config().desktop_view());
            else
                connect_data.setDesktopConfig(ComputerFactory::defaultDesktopViewConfig());
        }
        break;

        default:
            break;
    }

    Client* client = new Client(connect_data, this);
    connect(client, &Client::clientTerminated, this, &ConsoleWindow::onClientTerminated);
    client_list_.push_back(client);
}

void ConsoleWindow::onClientTerminated(Client* client)
{
    for (auto it = client_list_.begin(); it != client_list_.end(); ++it)
    {
        if (client == *it)
        {
            client_list_.erase(it);
            break;
        }
    }

    delete client;
}

} // namespace aspia
