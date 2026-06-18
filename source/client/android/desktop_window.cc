//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/android/desktop_window.h"

#include <QGridLayout>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "client/client_desktop.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router.h"
#include "client/session_state.h"
#include "client/settings.h"
#include "client/android/desktop_view.h"
#include "client/android/key_bar.h"
#include "common/android/bottom_sheet.h"
#include "common/android/floating_action_button.h"
#include "common/android/label.h"
#include "common/android/message_dialog.h"
#include "common/clipboard.h"
#include "common/desktop_session_constants.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_power.h"
#include "proto/desktop_control.h"
#include "proto/desktop_input.h"
#include "proto/peer.h"
#include "proto/router_client.h"

namespace {

constexpr int kFabMargin = 16;

// The status text is limited to this fraction of the window width, so short messages stay on one
// line and only a genuinely long one wraps.
constexpr double kStatusWidthFactor = 0.9;

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopWindow::DesktopWindow(const HostConfig& host, QWidget* parent)
    : QWidget(parent),
      host_(host),
      view_(new DesktopView()),
      status_(new Label(QString(), Label::Role::BODY))
{
    // The status text floats centered over the desktop area until the first frame arrives.
    status_->setAlignment(Qt::AlignCenter);
    status_->setWordWrap(true);
    status_->setStyleSheet("color: white;");

    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(view_, 0, 0);
    layout->addWidget(status_, 0, 0, Qt::AlignCenter);

    fab_ = new FloatingActionButton(":/img/material/more_vert.svg", this);
    connect(fab_, &FloatingActionButton::sig_clicked, this, &DesktopWindow::onShowActions);

    // Key bar with modifiers and special keys, shown above the keyboard while it is open.
    key_bar_ = new KeyBar(this);
    key_bar_->hide();
    view_->setKeyBarHeight(key_bar_->height());

    connect(view_, &DesktopView::sig_keyboardInsetChanged, this, &DesktopWindow::onKeyboardInsetChanged);
    connect(view_, &DesktopView::sig_modifiersCleared, key_bar_, &KeyBar::clearModifiers);
    connect(key_bar_, &KeyBar::sig_modifierToggled, view_, &DesktopView::onModifierToggled);
    connect(key_bar_, &KeyBar::sig_specialKey, view_, &DesktopView::onSpecialKey);

    connect(GuiApplication::instance(), &QGuiApplication::applicationStateChanged,
            this, &DesktopWindow::onApplicationStateChanged);

    start();
}

//--------------------------------------------------------------------------------------------------
DesktopWindow::~DesktopWindow()
{
    client_.reset();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (status_)
        status_->setFixedWidth(qRound(width() * kStatusWidthFactor));

    if (fab_)
        fab_->move(width() - fab_->width() - kFabMargin, height() - fab_->height() - kFabMargin);

    // Keep the key bar spanning the width; its vertical position is set when the keyboard changes.
    if (key_bar_ && key_bar_->isVisible())
        key_bar_->setGeometry(0, key_bar_->y(), width(), key_bar_->height());
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::start()
{
    // An empty user name means a connection by ID with a one-time password (#host_id).
    if (host_.username().isEmpty())
        host_.setUsername(u"#" + host_.address());

    session_state_ = std::make_shared<SessionState>(
        host_, proto::peer::SESSION_TYPE_DESKTOP, Database::instance().displayName());

    setStatusText(tr("Connecting..."));

    if (session_state_->isConnectionByHostId())
        fetchConnectionOffer();
    else
        startNewClient();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::fetchConnectionOffer()
{
    Router* router = Router::instance(session_state_->routerId());
    if (!router)
    {
        setStatusText(tr("The specified router is unavailable."));
        return;
    }

    if (router->status() == Router::Status::ONLINE)
    {
        requestConnectionOffer(router);
        return;
    }

    // The router connection is also dropped while the app is in the background.
    setStatusText(tr("Connecting to router..."));

    // Drop any previous pending wait, then subscribe again.
    disconnect(router, &Router::sig_statusChanged, this, nullptr);
    connect(router, &Router::sig_statusChanged, this,
        [this](qint64 /* router_id */, Router::Status status)
    {
        if (status != Router::Status::ONLINE)
            return;

        Router* router = Router::instance(session_state_->routerId());
        if (!router)
            return;

        disconnect(router, &Router::sig_statusChanged, this, nullptr);
        requestConnectionOffer(router);
    });

    if (router->status() == Router::Status::OFFLINE)
        router->connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::requestConnectionOffer(Router* router)
{
    session_state_->setRouterVersion(router->version());
    setStatusText(tr("Requesting connection to the host..."));

    router->requestConnection(session_state_->hostId(), this,
        [this](const proto::router::ConnectionOffer& offer)
    {
        if (offer.error_code() == proto::router::ConnectionOffer::SUCCESS)
        {
            session_state_->setConnectionOffer(offer);
            startNewClient();
            return;
        }

        QString error;
        switch (offer.error_code())
        {
            case proto::router::ConnectionOffer::PEER_NOT_FOUND:
                error = tr("The host with the specified ID is not online.");
                break;
            case proto::router::ConnectionOffer::ACCESS_DENIED:
                error = tr("Access is denied.");
                break;
            case proto::router::ConnectionOffer::KEY_POOL_EMPTY:
                error = tr("There are no relays available or the key pool is empty.");
                break;
            default:
                error = tr("Error requesting connection via router.");
                break;
        }
        setStatusText(error);
    });
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::startNewClient()
{
    const proto::control::Config config = Settings().desktopConfig();
    ClientDesktop* client = new ClientDesktop(config);

    // The clipboard lives here on the GUI thread; the client runs on the I/O thread, so the
    // connections are queued. Created only when enabled to avoid monitoring the local clipboard.
    if (config.clipboard())
    {
        Clipboard* clipboard = Clipboard::create(this);
        if (clipboard)
        {
            connect(clipboard, &Clipboard::sig_clipboardEvent,
                    client, &ClientDesktop::onClipboardEvent, Qt::QueuedConnection);
            connect(clipboard, &Clipboard::sig_localFileListChanged,
                    client, &ClientDesktop::onClipboardLocalFileListChanged, Qt::QueuedConnection);
            connect(clipboard, &Clipboard::sig_fileDataRequest,
                    client, &ClientDesktop::onClipboardFileDataRequest, Qt::QueuedConnection);
            connect(client, &ClientDesktop::sig_injectClipboardEvent,
                    clipboard, &Clipboard::injectClipboardEvent, Qt::QueuedConnection);
            connect(client, &ClientDesktop::sig_clipboardFileData,
                    clipboard, &Clipboard::addFileData, Qt::QueuedConnection);
            clipboard->start();
        }
    }

    connect(client, &ClientDesktop::sig_frameChanged,
            this, &DesktopWindow::onFrameChanged, Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_drawFrame, view_, &DesktopView::refresh, Qt::QueuedConnection);
    connect(client, &Client::sig_statusChanged,
            this, &DesktopWindow::onStatusChanged, Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_mouseCursorChanged,
            view_, &DesktopView::setCursorShape, Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_screenListChanged,
            this, &DesktopWindow::onScreenListChanged, Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_capabilities,
            this, &DesktopWindow::onCapabilitiesChanged, Qt::QueuedConnection);
    connect(view_, &DesktopView::sig_mouseEvent,
            client, &ClientDesktop::onMouseEvent, Qt::QueuedConnection);
    connect(view_, &DesktopView::sig_keyEvent,
            client, &ClientDesktop::onKeyEvent, Qt::QueuedConnection);
    connect(view_, &DesktopView::sig_textEvent,
            client, &ClientDesktop::onTextEvent, Qt::QueuedConnection);
    connect(this, &DesktopWindow::sig_screenSelected,
            client, &ClientDesktop::onCurrentScreenChanged, Qt::QueuedConnection);
    connect(this, &DesktopWindow::sig_powerControl,
            client, &ClientDesktop::onPowerControl, Qt::QueuedConnection);

    client->moveToThread(GuiApplication::ioThread());
    client->setSessionState(session_state_);
    client_ = client;

    QMetaObject::invokeMethod(client, &Client::start, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::reconnect()
{
    LOG(INFO) << "Reconnecting after returning to foreground";
    client_.reset();
    start();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onStatusChanged(Client::Status status, const QVariant& data)
{
    switch (status)
    {
        case Client::Status::HOST_CONNECTING:
            setStatusText(tr("Connecting to host %1...").arg(session_state_->hostAddress()));
            break;

        case Client::Status::HOST_CONNECTED:
            connected_ = true;
            was_connected_ = true;
            setStatusText(tr("Connection established."));
            break;

        case Client::Status::HOST_DISCONNECTED:
        {
            QString message = tr("The connection to the host has been lost.");
            if (data.canConvert<TcpChannel::ErrorCode>())
                message = TcpChannel::errorToString(data.value<TcpChannel::ErrorCode>());
            setStatusText(message);
            view_->setFrame(SharedFrame());
            connected_ = false;
        }
        break;

        case Client::Status::NO_ROUTER:
            setStatusText(tr("The specified router is unavailable."));
            break;

        case Client::Status::ROUTER_OFFLINE:
            setStatusText(tr("The specified router is offline."));
            break;

        case Client::Status::ROUTER_ERROR:
            setStatusText(tr("Error requesting connection via router: %1.").arg(data.toString()));
            break;

        case Client::Status::VERSION_MISMATCH:
            setStatusText(tr("The host version is newer than the client. Please update the application."));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onFrameChanged(const QSize& /* screen_size */, SharedFrame frame)
{
    view_->setFrame(std::move(frame));
    status_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onScreenListChanged(const proto::screen::ScreenList& screen_list)
{
    screen_list_ = screen_list;

    // The current screen can change from another client while our action sheet is open, so refresh
    // its highlight to match.
    if (action_sheet_)
    {
        int selected = -1;
        for (int i = 0; i < screen_list_.screen_size(); ++i)
        {
            if (screen_list_.screen(i).id() == screen_list_.current_screen())
            {
                selected = i;
                break;
            }
        }
        action_sheet_->setSelected(selected);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onCapabilitiesChanged(const proto::control::Capabilities& capabilities)
{
    for (int i = 0; i < capabilities.flag_size(); ++i)
    {
        const std::string& name = capabilities.flag(i).name();
        if (name == kFlagOSWindows)
            host_is_windows_ = true;
        else if (name == kFlagPowerControl)
            power_control_available_ = true;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onShowActions()
{
    action_sheet_ = new BottomSheet(this);

    const int screen_count = screen_list_.screen_size() > 1 ? screen_list_.screen_size() : 0;

    for (int i = 0; i < screen_count; ++i)
    {
        const bool selected = screen_list_.screen(i).id() == screen_list_.current_screen();
        action_sheet_->addItem(tr("Monitor %1").arg(i + 1), ":/img/material/monitor.svg", selected);
    }

    int next_index = screen_count;

    int power_index = -1;
    if (power_control_available_)
    {
        power_index = next_index++;
        action_sheet_->addItem(tr("Power"), ":/img/material/power.svg");
    }

    const int keyboard_index = next_index++;
    action_sheet_->addItem(tr("Keyboard"), ":/img/material/keyboard.svg");

    int ctrl_alt_del_index = -1;
    if (host_is_windows_)
    {
        ctrl_alt_del_index = next_index++;
        action_sheet_->addItem(tr("Ctrl+Alt+Del"), ":/img/material/lock.svg");
    }

    const int disconnect_index = next_index;
    action_sheet_->addItem(tr("Disconnect"), ":/img/material/close.svg");

    connect(action_sheet_, &BottomSheet::sig_triggered, this,
            [this, power_index, keyboard_index, ctrl_alt_del_index, disconnect_index](int index)
    {
        if (index == power_index)
            showPowerActions();
        else if (index == keyboard_index)
            view_->showSoftwareKeyboard();
        else if (index == ctrl_alt_del_index)
            view_->sendCtrlAltDelete();
        else if (index == disconnect_index)
            emit sig_closed();
        else if (index >= 0 && index < screen_list_.screen_size())
            emit sig_screenSelected(screen_list_.screen(index));
    });

    action_sheet_->showSheet();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::showPowerActions()
{
    BottomSheet* sheet = new BottomSheet(this);

    // A leading back item returns to the main action sheet instead of a title.
    sheet->addItem(tr("Back"), ":/img/material/arrow_back.svg");
    sheet->addItem(tr("Shutdown"), ":/img/material/power.svg");
    sheet->addItem(tr("Reboot"), ":/img/material/restart.svg");
    sheet->addItem(tr("Safe Mode"), ":/img/material/restart.svg");
    sheet->addItem(tr("Logoff"), ":/img/material/logout.svg");
    sheet->addItem(tr("Lock"), ":/img/material/lock.svg");

    connect(sheet, &BottomSheet::sig_triggered, this, [this](int index)
    {
        switch (index)
        {
            case 0:
                onShowActions();
                break;
            case 1:
                triggerPowerAction(proto::power::Control::ACTION_SHUTDOWN,
                    tr("Are you sure you want to shutdown the remote computer?"));
                break;
            case 2:
                triggerPowerAction(proto::power::Control::ACTION_REBOOT,
                    tr("Are you sure you want to reboot the remote computer?"));
                break;
            case 3:
                triggerPowerAction(proto::power::Control::ACTION_REBOOT_SAFE_MODE,
                    tr("Are you sure you want to reboot the remote computer in Safe Mode?"));
                break;
            case 4:
                triggerPowerAction(proto::power::Control::ACTION_LOGOFF,
                    tr("Are you sure you want to end the user session on the remote computer?"));
                break;
            case 5:
                triggerPowerAction(proto::power::Control::ACTION_LOCK,
                    tr("Are you sure you want to lock the user session on the remote computer?"));
                break;
            default:
                break;
        }
    });

    sheet->showSheet();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::triggerPowerAction(
    proto::power::Control::Action action, const QString& confirm_text)
{
    if (MessageDialog::confirm(this, tr("Confirmation"), confirm_text, tr("Yes")))
        emit sig_powerControl(action);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onKeyboardInsetChanged(int inset)
{
    if (inset <= 0)
    {
        key_bar_->hide();
        return;
    }

    const int bar_height = key_bar_->height();
    key_bar_->setGeometry(0, height() - inset - bar_height, width(), bar_height);
    key_bar_->raise();
    key_bar_->show();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onApplicationStateChanged(Qt::ApplicationState state)
{
    // The connection was established before but is now down (Android dropped it while backgrounded),
    // so reconnect as soon as the app is active again.
    if (state == Qt::ApplicationActive && was_connected_ && !connected_)
        reconnect();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::setStatusText(const QString& text)
{
    status_->setText(text);
    status_->setVisible(true);
}
