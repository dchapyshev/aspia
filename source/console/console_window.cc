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

#include "console/console_window.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTranslator>

#include "base/logging.h"
#include "client/ui/authorization_dialog.h"
#include "client/ui/client_dialog.h"
#include "client/client.h"
#include "client/config_factory.h"
#include "console/about_dialog.h"
#include "console/address_book_tab.h"
#include "console/console_settings.h"
#include "build_config.h"

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
    DISALLOW_COPY_AND_ASSIGN(LanguageAction);
};

} // namespace

ConsoleWindow::ConsoleWindow(const QString& file_path, QWidget* parent)
    : QMainWindow(parent),
      connections_(this)
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

    ui.action_show_tray_icon->setChecked(settings.alwaysShowTrayIcon());
    ui.action_minimize_to_tray->setChecked(settings.minimizeToTray());
    ui.action_toolbar->setChecked(settings.isToolBarEnabled());
    ui.action_statusbar->setChecked(settings.isStatusBarEnabled());

    ui.status_bar->setVisible(ui.action_statusbar->isChecked());
    showTrayIcon(ui.action_show_tray_icon->isChecked());

    connect(ui.action_show_hide, &QAction::triggered, this, &ConsoleWindow::onShowHideToTray);
    connect(ui.action_show_tray_icon, &QAction::toggled, this, &ConsoleWindow::showTrayIcon);
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
      
    connect(ui.action_system_info_connect, &QAction::triggered,
            this, &ConsoleWindow::onSystemInfoConnect);

    connect(ui.tool_bar, &QToolBar::visibilityChanged, ui.action_toolbar, &QAction::setChecked);
    connect(ui.action_toolbar, &QAction::toggled, ui.tool_bar, &QToolBar::setVisible);
    connect(ui.action_statusbar, &QAction::toggled, ui.status_bar, &QStatusBar::setVisible);
    connect(ui.tab_widget, &QTabWidget::currentChanged, this, &ConsoleWindow::onCurrentTabChanged);
    connect(ui.tab_widget, &QTabWidget::tabCloseRequested, this, &ConsoleWindow::onCloseTab);
    connect(ui.menu_language, &QMenu::triggered, this, &ConsoleWindow::onLanguageChanged);

    QActionGroup* session_type_group = new QActionGroup(this);

    session_type_group->addAction(ui.action_desktop_manage);
    session_type_group->addAction(ui.action_desktop_view);
    session_type_group->addAction(ui.action_file_transfer);
    session_type_group->addAction(ui.action_system_info);

    switch (settings.sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            ui.action_desktop_manage->setChecked(true);
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            ui.action_desktop_view->setChecked(true);
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            ui.action_file_transfer->setChecked(true);
            break;

        case proto::SESSION_TYPE_SYSTEM_INFO:
            ui.action_system_info->setChecked(true);
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
#if defined(OS_WIN)
            if (file_path.compare(tab->addressBookPath(), Qt::CaseInsensitive) == 0)
#else
            if (file_path.compare(tab->addressBookPath(), Qt::CaseSensitive) == 0)
#endif // defined(OS_WIN)
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
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->save();
}

void ConsoleWindow::onSaveAsAddressBook()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->saveAs();
}

void ConsoleWindow::onCloseAddressBook()
{
    onCloseTab(ui.tab_widget->currentIndex());
}

void ConsoleWindow::onAddressBookProperties()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyAddressBook();
}

void ConsoleWindow::onAddComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->addComputer();
}

void ConsoleWindow::onModifyComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyComputer();
}

void ConsoleWindow::onDeleteComputer()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->removeComputer();
}

void ConsoleWindow::onAddComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->addComputerGroup();
}

void ConsoleWindow::onModifyComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->modifyComputerGroup();
}

void ConsoleWindow::onDeleteComputerGroup()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
        tab->removeComputerGroup();
}

void ConsoleWindow::onOnlineHelp()
{
    QDesktopServices::openUrl(QUrl(tr("https://aspia.org/en/help.html")));
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
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        proto::address_book::Computer* computer = tab->currentComputer();
        if (computer)
        {
            computer->set_connect_time(QDateTime::currentSecsSinceEpoch());
            computer->set_session_type(proto::SESSION_TYPE_DESKTOP_MANAGE);

            connectToComputer(*computer);
        }
    }
}

void ConsoleWindow::onDesktopViewConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        proto::address_book::Computer* computer = tab->currentComputer();
        if (computer)
        {
            computer->set_connect_time(QDateTime::currentSecsSinceEpoch());
            computer->set_session_type(proto::SESSION_TYPE_DESKTOP_VIEW);

            connectToComputer(*computer);
        }
    }
}

void ConsoleWindow::onFileTransferConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        proto::address_book::Computer* computer = tab->currentComputer();
        if (computer)
        {
            computer->set_connect_time(QDateTime::currentSecsSinceEpoch());
            computer->set_session_type(proto::SESSION_TYPE_FILE_TRANSFER);

            connectToComputer(*computer);
        }
    }
}

void ConsoleWindow::onSystemInfoConnect()
{
    AddressBookTab* tab = currentAddressBookTab();
    if (tab)
    {
        proto::address_book::Computer* computer = tab->currentComputer();
        if (computer)
        {
            computer->set_connect_time(QDateTime::currentSecsSinceEpoch());
            computer->set_session_type(proto::SESSION_TYPE_SYSTEM_INFO);

            connectToComputer(*computer);
        }
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

    AddressBookTab* tab = currentAddressBookTab();
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
        computer->set_session_type(proto::SESSION_TYPE_DESKTOP_MANAGE);
    }
    else if (ui.action_desktop_view->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_DESKTOP_VIEW);
    }
    else if (ui.action_file_transfer->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_FILE_TRANSFER);
    }
    else if (ui.action_system_info->isChecked())
    {
        computer->set_session_type(proto::SESSION_TYPE_SYSTEM_INFO);
    }
    else
    {
        LOG(LS_FATAL) << "Unknown session type";
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

void ConsoleWindow::onShowHideToTray()
{
    if (isHidden())
    {
        ui.action_show_hide->setText(tr("Hide"));

        if (!ui.action_show_tray_icon->isChecked())
            showTrayIcon(false);

        if (windowState() & Qt::WindowMaximized)
            showMaximized();
        else
            showNormal();

        activateWindow();
        setFocus();
    }
    else
    {
        ui.action_show_hide->setText(tr("Show"));

        if (!ui.action_show_tray_icon->isChecked())
            showTrayIcon(true);

        hide();
    }
}

void ConsoleWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && ui.action_minimize_to_tray->isChecked())
    {
        if (windowState() & Qt::WindowMinimized)
            onShowHideToTray();
    }

    QMainWindow::changeEvent(event);
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
                    event->ignore();
                    return;

                default:
                    break;
            }
        }
    }

    ConsoleSettings settings;
    settings.setToolBarEnabled(ui.action_toolbar->isChecked());
    settings.setStatusBarEnabled(ui.action_statusbar->isChecked());
    settings.setAlwaysShowTrayIcon(ui.action_show_tray_icon->isChecked());
    settings.setMinimizeToTray(ui.action_minimize_to_tray->isChecked());
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());

    if (ui.action_desktop_manage->isChecked())
        settings.setSessionType(proto::SESSION_TYPE_DESKTOP_MANAGE);
    else if (ui.action_desktop_view->isChecked())
        settings.setSessionType(proto::SESSION_TYPE_DESKTOP_VIEW);
    else if (ui.action_file_transfer->isChecked())
        settings.setSessionType(proto::SESSION_TYPE_FILE_TRANSFER);
    else if (ui.action_system_info->isChecked())
        settings.setSessionType(proto::SESSION_TYPE_SYSTEM_INFO);

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

void ConsoleWindow::showTrayIcon(bool show)
{
    if (show)
    {
        tray_menu_.reset(new QMenu(this));
        tray_menu_->addAction(ui.action_show_hide);
        tray_menu_->addAction(ui.action_exit);

        tray_icon_.reset(new QSystemTrayIcon(this));
        tray_icon_->setIcon(QIcon(QStringLiteral(":/icon/main.png")));
        tray_icon_->setToolTip(tr("Aspia Console"));
        tray_icon_->setContextMenu(tray_menu_.get());
        tray_icon_->show();

        connect(tray_icon_.get(), &QSystemTrayIcon::activated,
                [this](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::Context)
                return;

            onShowHideToTray();
        });
    }
    else
    {
        tray_icon_.reset();
        tray_menu_.reset();
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

AddressBookTab* ConsoleWindow::currentAddressBookTab()
{
    int current_tab = ui.tab_widget->currentIndex();
    if (current_tab == -1)
        return nullptr;

    return dynamic_cast<AddressBookTab*>(ui.tab_widget->widget(current_tab));
}

void ConsoleWindow::connectToComputer(const proto::address_book::Computer& computer)
{
    ConnectData connect_data;

    connect_data.setComputerName(QString::fromStdString(computer.name()));
    connect_data.setAddress(QString::fromStdString(computer.address()));
    connect_data.setPort(computer.port());
    connect_data.setUserName(QString::fromStdString(computer.username()));
    connect_data.setPassword(QString::fromStdString(computer.password()));
    connect_data.setSessionType(computer.session_type());

    if (connect_data.userName().isEmpty() || connect_data.password().isEmpty())
    {
        AuthorizationDialog auth_dialog(this);

        auth_dialog.setUserName(connect_data.userName());
        auth_dialog.setPassword(connect_data.password());

        if (auth_dialog.exec() == AuthorizationDialog::Rejected)
            return;

        connect_data.setUserName(auth_dialog.userName());
        connect_data.setPassword(auth_dialog.password());
    }

    switch (computer.session_type())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        {
            if (computer.session_config().has_desktop_manage())
                connect_data.setDesktopConfig(computer.session_config().desktop_manage());
            else
                connect_data.setDesktopConfig(ConfigFactory::defaultDesktopManageConfig());
        }
        break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            if (computer.session_config().has_desktop_view())
                connect_data.setDesktopConfig(computer.session_config().desktop_view());
            else
                connect_data.setDesktopConfig(ConfigFactory::defaultDesktopViewConfig());
        }
        break;

        default:
            break;
    }

    connections_.connectWith(connect_data);
}

} // namespace aspia
