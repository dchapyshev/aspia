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

#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "host/ui/host_config_dialog.h"
#include "host/ui/host_notifier_window.h"
#include "host/host_settings.h"
#include "host/host_ui_client.h"
#include "qt_base/application.h"
#include "qt_base/qt_logging.h"
#include "qt_base/xml_settings.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QUrl>

namespace host {

MainWindow::MainWindow(Settings& settings, QWidget* parent)
    : QMainWindow(parent),
      settings_(settings)
{
    ui.setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    QString text = tr("Not available");
    ui.edit_id->setText(text);
    ui.edit_ip->setText(text);

    tray_menu_.addAction(ui.action_settings);
    tray_menu_.addSeparator();
    tray_menu_.addAction(ui.action_show_hide);
    tray_menu_.addAction(ui.action_exit);

    tray_icon_.setIcon(QIcon(QStringLiteral(":/img/main.png")));
    tray_icon_.setToolTip(tr("Aspia Host"));
    tray_icon_.setContextMenu(&tray_menu_);
    tray_icon_.show();

    createLanguageMenu(settings.locale());

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

    setFixedHeight(sizeHint().height());
}

MainWindow::~MainWindow() = default;

void MainWindow::connectToService()
{
    if (client_ && client_->state() != UiClient::State::NOT_CONNECTED)
        return;

    client_ = new UiClient(this);

    connect(ui.button_new_password, &QPushButton::released, client_, &UiClient::newPassword);
    connect(ui.button_refresh_ip_list, &QPushButton::released, client_, &UiClient::refresh);
    connect(client_, &UiClient::credentialsReceived, this, &MainWindow::onCredentialsReceived);

    connect(client_, &UiClient::connected, [this]()
    {
        ui.button_new_password->setEnabled(true);
        ui.button_refresh_ip_list->setEnabled(true);
    });

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

void MainWindow::onCredentialsReceived(const proto::host::Credentials& credentials)
{
    std::string ip;

    for (int i = 0; i < credentials.ip_size(); ++i)
    {
        if (!ip.empty())
            ip += ", ";

        ip += credentials.ip(i);
    }

    id_ = credentials.id();
    ip_ = std::move(ip);
    username_ = credentials.username();
    password_ = credentials.password();

    refeshCredentials();
}

void MainWindow::onLanguageChanged(QAction* action)
{
    common::LanguageAction* language_action = dynamic_cast<common::LanguageAction*>(action);
    if (!language_action)
        return;

    QString new_locale = language_action->locale();

    qt_base::Application::instance()->setLocale(new_locale);

    ui.retranslateUi(this);
    refeshCredentials();

    settings_.setLocale(new_locale);
}

void MainWindow::onSettings()
{
    QApplication::setQuitOnLastWindowClosed(false);

    ConfigDialog(this).exec();

    QApplication::setQuitOnLastWindowClosed(true);
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
    QApplication::setQuitOnLastWindowClosed(false);

    common::AboutDialog(this).exec();

    QApplication::setQuitOnLastWindowClosed(true);
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

    for (const auto& locale : qt_base::Application::instance()->localeList())
    {
        common::LanguageAction* action_language =
            new common::LanguageAction(locale.first, locale.second, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale.first)
            action_language->setChecked(true);

        ui.menu_language->addAction(action_language);
    }
}

void MainWindow::refeshCredentials()
{
    bool has_id = !id_.empty();

    ui.label_icon_id->setEnabled(has_id);
    ui.label_id->setEnabled(has_id);
    ui.edit_id->setEnabled(has_id);
    ui.edit_id->setText(has_id ? QString::fromStdString(id_) : tr("Not available"));

    bool has_ip = !ip_.empty();

    ui.label_icon_ip->setEnabled(has_ip);
    ui.label_ip->setEnabled(has_ip);
    ui.edit_ip->setEnabled(has_ip);
    ui.edit_ip->setText(has_ip ? QString::fromStdString(ip_) : tr("Not available"));

    bool has_username = !username_.empty();

    ui.label_icon_user->setEnabled(has_username);
    ui.label_user->setEnabled(has_username);
    ui.edit_user->setEnabled(has_username);
    ui.edit_user->setText(has_username ? QString::fromStdString(username_) : QString());

    bool has_password = !password_.empty();

    ui.label_icon_password->setEnabled(has_password);
    ui.label_password->setEnabled(has_password);
    ui.edit_password->setEnabled(has_password);
    ui.edit_password->setText(has_password ? QString::fromStdString(password_) : QString());
}

} // namespace host
