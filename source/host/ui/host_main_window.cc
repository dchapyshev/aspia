//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "host/user_session_agent.h"
#include "host/user_session_agent_proxy.h"
#include "host/user_session_window_proxy.h"
#include "host/ui/host_application.h"
#include "host/ui/host_config_dialog.h"
#include "host/ui/host_notifier_window.h"
#include "qt_base/qt_logging.h"
#include "qt_base/qt_xml_settings.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QUrl>

namespace host {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      window_proxy_(std::make_shared<UserSessionWindowProxy>(
          qt_base::Application::taskRunner(), this))
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

    createLanguageMenu(Application::instance()->settings().locale());

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

    connect(ui.button_new_password, &QPushButton::released, [this]()
    {
        if (agent_proxy_)
            agent_proxy_->updateCredentials(proto::internal::CredentialsRequest::NEW_PASSWORD);
    });

    connect(ui.button_refresh_ip_list, &QPushButton::released, [this]()
    {
        if (agent_proxy_)
            agent_proxy_->updateCredentials(proto::internal::CredentialsRequest::REFRESH);
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::connectToService()
{
    agent_ = std::make_unique<UserSessionAgent>(window_proxy_);
    agent_->start();
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
        window_proxy_->dettach();

        if (notifier_)
            notifier_->close();

        QApplication::quit();
    }
}

void MainWindow::onStateChanged(UserSessionAgent::State state)
{
    if (state == UserSessionAgent::State::CONNECTED)
    {
        LOG(LS_INFO) << "The connection to the service was successfully established";

        ui.button_new_password->setEnabled(true);
        ui.button_refresh_ip_list->setEnabled(true);

        agent_proxy_ = agent_->agentProxy();
        agent_proxy_->updateCredentials(proto::internal::CredentialsRequest::REFRESH);
    }
    else
    {
        DCHECK_EQ(state, UserSessionAgent::State::DISCONNECTED);

        LOG(LS_INFO) << "The connection to the service is lost. The application will be closed";
        realClose();
    }
}

void MainWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!notifier_)
    {
        notifier_ = new NotifierWindow();

        connect(notifier_, &NotifierWindow::killSession, [this](const std::string& uuid)
        {
            if (agent_proxy_)
                agent_proxy_->killClient(uuid);
        });

        notifier_->setAttribute(Qt::WA_DeleteOnClose);
        notifier_->show();
        notifier_->activateWindow();
    }

    notifier_->onClientListChanged(clients);
}

void MainWindow::onCredentialsChanged(const proto::internal::Credentials& credentials)
{
    std::string ip;

    for (int i = 0; i < credentials.ip_size(); ++i)
    {
        if (!ip.empty())
            ip += ", ";

        ip += credentials.ip(i);
    }

    bool has_id = !credentials.id().empty();

    ui.label_icon_id->setEnabled(has_id);
    ui.label_id->setEnabled(has_id);
    ui.edit_id->setEnabled(has_id);
    ui.edit_id->setText(has_id ? QString::fromStdString(credentials.id()) : tr("Not available"));

    bool has_ip = !ip.empty();

    ui.label_icon_ip->setEnabled(has_ip);
    ui.label_ip->setEnabled(has_ip);
    ui.edit_ip->setEnabled(has_ip);
    ui.edit_ip->setText(has_ip ? QString::fromStdString(ip) : tr("Not available"));

    bool has_username = !credentials.username().empty();

    ui.label_icon_user->setEnabled(has_username);
    ui.label_user->setEnabled(has_username);
    ui.edit_user->setEnabled(has_username);
    ui.edit_user->setText(
        has_username ? QString::fromStdString(credentials.username()) : QString());

    bool has_password = !credentials.password().empty();

    ui.label_icon_password->setEnabled(has_password);
    ui.label_password->setEnabled(has_password);
    ui.edit_password->setEnabled(has_password);
    ui.edit_password->setText(
        has_password ? QString::fromStdString(credentials.password()) : QString());
}

void MainWindow::realClose()
{
    should_be_quit_ = true;
    close();
}

void MainWindow::onLanguageChanged(QAction* action)
{
    QString new_locale = static_cast<common::LanguageAction*>(action)->locale();
    Application* application = Application::instance();

    application->settings().setLocale(new_locale);
    application->setLocale(new_locale);

    ui.retranslateUi(this);

    if (agent_proxy_)
        agent_proxy_->updateCredentials(proto::internal::CredentialsRequest::REFRESH);
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

} // namespace host
