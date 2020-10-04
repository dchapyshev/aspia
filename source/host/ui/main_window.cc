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

#include "host/ui/main_window.h"

#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/strings/unicode.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "common/ui/status_dialog.h"
#include "host/user_session_agent.h"
#include "host/user_session_agent_proxy.h"
#include "host/user_session_window_proxy.h"
#include "host/ui/application.h"
#include "host/ui/config_dialog.h"
#include "host/ui/notifier_window.h"
#include "qt_base/qt_logging.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

namespace host {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      window_proxy_(std::make_shared<UserSessionWindowProxy>(
          qt_base::Application::uiTaskRunner(), this))
{
    ui.setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    ui.edit_id->setText("-");
    ui.edit_password->setText("-");

    tray_menu_.addAction(ui.action_settings);
    tray_menu_.addSeparator();
    tray_menu_.addAction(ui.action_show_hide);
    tray_menu_.addAction(ui.action_exit);

    tray_icon_.setIcon(QIcon(":/img/main.png"));
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
}

MainWindow::~MainWindow() = default;

void MainWindow::connectToService()
{
    agent_proxy_ = std::make_unique<UserSessionAgentProxy>(
        qt_base::Application::ioTaskRunner(), std::make_unique<UserSessionAgent>(window_proxy_));

    agent_proxy_->start();
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

void MainWindow::onStatusChanged(UserSessionAgent::Status status)
{
    if (status == UserSessionAgent::Status::CONNECTED_TO_SERVICE)
    {
        LOG(LS_INFO) << "The connection to the service was successfully established.";
    }
    else if (status == UserSessionAgent::Status::DISCONNECTED_FROM_SERVICE)
    {
        agent_proxy_.reset();

        LOG(LS_INFO) << "The connection to the service is lost. The application will be closed.";
        realClose();
    }
    else if (status == UserSessionAgent::Status::SERVICE_NOT_AVAILABLE)
    {
        agent_proxy_.reset();

        LOG(LS_INFO) << "The connection to the service has not been established. "
                     << "The application works offline.";
    }
    else
    {
        LOG(LS_WARNING) << "Unandled status code: " << static_cast<int>(status);
    }
}

void MainWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!notifier_)
    {
        notifier_ = new NotifierWindow();

        connect(notifier_, &NotifierWindow::killSession, [this](uint32_t id)
        {
            if (agent_proxy_)
                agent_proxy_->killClient(id);
        });

        notifier_->setAttribute(Qt::WA_DeleteOnClose);
        notifier_->show();
        notifier_->activateWindow();
    }

    notifier_->onClientListChanged(clients);
}

void MainWindow::onCredentialsChanged(const proto::internal::Credentials& credentials)
{
    ui.button_new_password->setEnabled(true);

    bool has_id = credentials.host_id() != base::kInvalidHostId;

    ui.label_icon_id->setEnabled(has_id);
    ui.label_id->setEnabled(has_id);
    ui.edit_id->setEnabled(has_id);
    ui.edit_id->setText(has_id ? QString::number(credentials.host_id()) : tr("Not available"));

    bool has_password = !credentials.password().empty();

    ui.label_icon_password->setEnabled(has_password);
    ui.label_password->setEnabled(has_password);
    ui.edit_password->setEnabled(has_password);
    ui.edit_password->setText(QString::fromStdString(credentials.password()));
}

void MainWindow::onRouterStateChanged(const proto::internal::RouterState& state)
{
    last_state_ = state.state();

    QString router;

    if (state.state() != proto::internal::RouterState::DISABLED)
    {
        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(base::utf16FromUtf8(state.host_name()));
        address.setPort(state.host_port());

        router = QString::fromStdU16String(address.toString());
    }

    if (state.state() == proto::internal::RouterState::DISABLED)
        ui.button_status->setEnabled(false);
    else
        ui.button_status->setEnabled(true);

    if (state.state() != proto::internal::RouterState::CONNECTED)
    {
        ui.label_icon_id->setEnabled(false);
        ui.label_icon_password->setEnabled(false);
        ui.button_new_password->setEnabled(false);

        ui.edit_id->setText("-");
        ui.edit_password->setText("-");
    }
    else
    {
        ui.button_status->setEnabled(true);
    }

    QString status;

    switch (state.state())
    {
        case proto::internal::RouterState::DISABLED:
            status = tr("Router is disabled");
            break;

        case proto::internal::RouterState::CONNECTING:
            status = tr("Connecting to a router %1...").arg(router);
            break;

        case proto::internal::RouterState::CONNECTED:
            status = tr("Connected to a router %1").arg(router);
            break;

        case proto::internal::RouterState::FAILED:
            status = tr("Failed to connect to router %1").arg(router);
            break;

        default:
            NOTREACHED();
            return;
    }

    updateStatusBar();

    if (!status_dialog_)
    {
        status_dialog_ = new common::StatusDialog(this);

        connect(ui.button_status, &QPushButton::clicked,
                status_dialog_, &common::StatusDialog::show);
    }

    status_dialog_->addMessage(status);
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
    updateStatusBar();

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
    // If the connection to the service is not established, then exit immediately.
    if (!agent_proxy_)
    {
        realClose();
        return;
    }

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

void MainWindow::updateStatusBar()
{
    QString message;
    QString icon;

    switch (last_state_)
    {
        case proto::internal::RouterState::DISABLED:
            message = tr("Router is disabled");
            icon = ":/img/cross-script.png";
            break;

        case proto::internal::RouterState::CONNECTING:
            message = tr("Connecting to a router...");
            icon = ":/img/arrow-circle-double.png";
            break;

        case proto::internal::RouterState::CONNECTED:
            message = tr("Connected to a router");
            icon = ":/img/tick.png";
            break;

        case proto::internal::RouterState::FAILED:
            message = tr("Connection error");
            icon = ":/img/cross-script.png";
            break;

        default:
            NOTREACHED();
            return;
    }

    ui.button_status->setText(message);
    ui.button_status->setIcon(QIcon(icon));
}

} // namespace host
