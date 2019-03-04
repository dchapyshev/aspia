//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/ui/host_main_window.h"

#include "base/qt_logging.h"
#include "base/xml_settings.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "host/ui/host_config_dialog.h"
#include "host/ui/host_notifier_window.h"
#include "host/host_settings.h"
#include "host/host_ui_client.h"
#include "host/password_generator.h"
#include "net/adapter_enumerator.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QUrl>

namespace host {

MainWindow::MainWindow(Settings& settings, common::LocaleLoader& locale_loader, QWidget* parent)
    : QMainWindow(parent),
      settings_(settings),
      locale_loader_(locale_loader)
{
    ui.setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    tray_menu_.addAction(ui.action_show_hide);
    tray_menu_.addSeparator();
    tray_menu_.addAction(ui.action_exit);

    tray_icon_.setIcon(QIcon(QStringLiteral(":/img/main.png")));
    tray_icon_.setToolTip(tr("Aspia Host"));
    tray_icon_.setContextMenu(&tray_menu_);
    tray_icon_.show();

    createLanguageMenu(settings.locale());
    refreshIpList();
    newPassword();

    connect(&tray_icon_, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Context)
            return;

        onShowHide();
    });

    connect(ui.menu_language, &QMenu::triggered, this, &MainWindow::onLanguageChanged);
    connect(ui.action_settings, &QAction::triggered, this, &MainWindow::onSettings);
    connect(ui.action_show_hide, &QAction::triggered, this, &MainWindow::onShowHide);
    connect(ui.action_exit, &QAction::triggered, this, &MainWindow::onExit);
    connect(ui.action_help, &QAction::triggered, this, &MainWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &MainWindow::onAbout);

    connect(ui.button_refresh_ip_list, &QPushButton::released, this, &MainWindow::refreshIpList);
    connect(ui.button_new_password, &QPushButton::released, this, &MainWindow::newPassword);

    setFixedHeight(sizeHint().height());


}

MainWindow::~MainWindow() = default;

void MainWindow::connectToService()
{
    if (!client_)
    {
        client_ = new UiClient(this);

        connect(client_, &UiClient::disconnected, this, &MainWindow::realClose);
        connect(client_, &UiClient::errorOccurred, client_, &UiClient::deleteLater);
        connect(client_, &UiClient::connectEvent, [this](const proto::host::ConnectEvent& event)
        {
            if (!notifier_)
            {
                notifier_ = new NotifierWindow();

                connect(client_, &UiClient::disconnectEvent,
                        notifier_, &NotifierWindow::onDisconnectEvent);

                connect(notifier_, &NotifierWindow::killSession,
                        client_, &UiClient::killSession);

                notifier_->setAttribute(Qt::WA_DeleteOnClose);
                notifier_->show();
                notifier_->activateWindow();
            }

            notifier_->onConnectEvent(event);
        });

        client_->start();
    }
}

void MainWindow::activateHost()
{
    show();
    activateWindow();
    connectToService();
}

void MainWindow::hideToTray()
{
    ui.action_show_hide->setText(tr("Show"));
    setVisible(false);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!should_be_quit_)
    {
        hideToTray();
        event->ignore();
    }
    else
    {
        if (notifier_)
            notifier_->close();

        QApplication::quit();
    }
}

void MainWindow::realClose()
{
    should_be_quit_ = true;
    close();
}

void MainWindow::refreshIpList()
{
    QString ip_list;

    for (net::AdapterEnumerator adapters; !adapters.isAtEnd(); adapters.advance())
    {
        for (net::AdapterEnumerator::IpAddressEnumerator addresses(adapters);
             !addresses.isAtEnd(); addresses.advance())
        {
            if (!ip_list.isEmpty())
                ip_list += QStringLiteral(", ");

            ip_list += QString::fromStdString(addresses.address());
        }
    }

    ui.edit_ip->setText(ip_list);
}

void MainWindow::newPassword()
{
    PasswordGenerator generator;

    generator.setLength(8);
    generator.setCharacters(PasswordGenerator::LOWER_CASE |
                            PasswordGenerator::UPPER_CASE |
                            PasswordGenerator::DIGITS);

    ui.edit_password->setText(generator.result());
}

void MainWindow::onLanguageChanged(QAction* action)
{
    common::LanguageAction* language_action = dynamic_cast<common::LanguageAction*>(action);
    if (!language_action)
        return;

    QString new_locale = language_action->locale();
    locale_loader_.installTranslators(new_locale);

    ui.retranslateUi(this);
    settings_.setLocale(new_locale);
}

void MainWindow::onSettings()
{
    ConfigDialog(this).exec();
}

void MainWindow::onShowHide()
{
    if (isVisible())
    {
        ui.action_show_hide->setText(tr("Show"));
        setVisible(false);
    }
    else
    {
        ui.action_show_hide->setText(tr("Hide"));
        setVisible(true);
    }
}

void MainWindow::onHelp()
{
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

void MainWindow::onAbout()
{
    common::AboutDialog(this).exec();
}

void MainWindow::onExit()
{
    QApplication::setQuitOnLastWindowClosed(false);

    int button = QMessageBox::question(
        this,
        tr("Confirmation"),
        tr("If you exit from Aspia, it will not be possible to connect to this computer until "
           "you turn on the computer or Aspia again manually. Do you really want to exit the "
           "application?"),
        QMessageBox::Yes,
        QMessageBox::No);

    QApplication::setQuitOnLastWindowClosed(true);

    if (button == QMessageBox::Yes)
    {
        if (!notifier_)
        {
            realClose();
        }
        else
        {
            connect(notifier_, &NotifierWindow::finished, this, &MainWindow::realClose);
            notifier_->disconnectAll();
        }
    }
}

void MainWindow::createLanguageMenu(const QString& current_locale)
{
    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : locale_loader_.sortedLocaleList())
    {
        common::LanguageAction* action_language = new common::LanguageAction(locale, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale)
            action_language->setChecked(true);

        ui.menu_language->addAction(action_language);
    }
}

} // namespace host
