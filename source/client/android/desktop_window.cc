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
#include <QPointF>
#include <QTimer>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/net/tcp_channel.h"
#include "base/threading/worker_manager.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router.h"
#include "client/session_keeper.h"
#include "client/session_state.h"
#include "client/settings.h"
#include "client/android/desktop_view.h"
#include "client/android/key_bar.h"
#include "client/android/statistics_dialog.h"
#include "client/workers/audio_worker.h"
#include "client/workers/network_worker.h"
#include "client/workers/video_worker.h"
#include "common/android/bottom_sheet.h"
#include "common/android/floating_action_button.h"
#include "common/android/label.h"
#include "common/android/message_dialog.h"
#include "common/clipboard.h"
#include "common/desktop_session_constants.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_control.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_input.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_video.h"
#include "proto/peer.h"
#include "proto/router_client.h"

namespace {

constexpr int kFabMargin = 16;

// The status text is limited to this fraction of the window width, so short messages stay on one
// line and only a genuinely long one wraps.
constexpr double kStatusWidthFactor = 0.9;

//--------------------------------------------------------------------------------------------------
// A session is offered for switching when it is the console session or an active one (matches the
// desktop client).
bool isSessionVisible(const proto::control::Session& session, const proto::control::SessionList& list)
{
    return session.session_id() == list.console_session_id() || session.is_active();
}

//--------------------------------------------------------------------------------------------------
QString formatSessionText(int index, const QString& user_name, bool is_console)
{
    QString text = user_name.isEmpty() ?
        QCoreApplication::translate("DesktopWindow", "Session %1").arg(index + 1) :
        QCoreApplication::translate("DesktopWindow", "Session %1 (%2)").arg(index + 1).arg(user_name);

    if (is_console)
        text.append("*");

    return text;
}

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

    connect(view_, &DesktopView::sig_mouseEvent, this, &DesktopWindow::onMouseEvent);
    connect(view_, &DesktopView::sig_keyEvent, this, &DesktopWindow::onKeyEvent);
    connect(view_, &DesktopView::sig_textEvent, this, &DesktopWindow::onTextEvent);
    connect(this, &DesktopWindow::sig_screenSelected, this, &DesktopWindow::onCurrentScreenChanged);
    connect(this, &DesktopWindow::sig_powerControl, this, &DesktopWindow::onPowerControl);
    connect(this, &DesktopWindow::sig_switchSession, this, &DesktopWindow::onSwitchSession);

    start();
}

//--------------------------------------------------------------------------------------------------
DesktopWindow::~DesktopWindow()
{
    if (session_keeper_)
        session_keeper_->release();

    worker_manager_.reset();
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
        startNewSession();
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
            startNewSession();
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
void DesktopWindow::startNewSession()
{
    desktop_config_ = Settings().desktopConfig();

    if (!session_keeper_)
        session_keeper_ = SessionKeeper::create(this);

    worker_manager_.reset();
    worker_manager_ = std::make_unique<WorkerManager>();

    std::unique_ptr<NetworkWorker> network_worker = std::make_unique<NetworkWorker>();
    network_worker_ = network_worker.get();
    worker_manager_->add(std::move(network_worker));

    std::unique_ptr<AudioWorker> audio_worker = std::make_unique<AudioWorker>();
    audio_worker_ = audio_worker.get();
    worker_manager_->add(std::move(audio_worker));

    std::unique_ptr<VideoWorker> video_worker = std::make_unique<VideoWorker>();
    video_worker_ = video_worker.get();
    worker_manager_->add(std::move(video_worker));

    connect(this, &DesktopWindow::sig_startConnection, network_worker_, &NetworkWorker::onStartConnection,
            Qt::QueuedConnection);
    connect(this, &DesktopWindow::sig_sessionReady, network_worker_, &NetworkWorker::onSessionReady,
            Qt::QueuedConnection);
    connect(this, &DesktopWindow::sig_sendMessage, network_worker_, &NetworkWorker::onSendMessage,
            Qt::QueuedConnection);
    connect(network_worker_, &NetworkWorker::sig_statusChanged, this, &DesktopWindow::onNetworkStatusChanged,
            Qt::QueuedConnection);
    connect(network_worker_, &NetworkWorker::sig_channel_2, this, &DesktopWindow::onScreenMessage,
            Qt::QueuedConnection);
    connect(network_worker_, &NetworkWorker::sig_channel_1, this, &DesktopWindow::onControlMessage,
            Qt::QueuedConnection);
    connect(network_worker_, &NetworkWorker::sig_channel_6, this, &DesktopWindow::onClipboardMessage,
            Qt::QueuedConnection);

    connect(this, &DesktopWindow::sig_cursorConfig, video_worker_, &VideoWorker::onCursorConfig,
            Qt::QueuedConnection);

    connect(video_worker_, &VideoWorker::sig_frameChanged,
            this, &DesktopWindow::onFrameChanged, Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_drawFrame,
            view_, &DesktopView::refresh, Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_mouseCursorChanged,
            view_, &DesktopView::setCursorShape, Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_cursorPositionChanged,
            this, &DesktopWindow::onCursorPositionChanged, Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_h264Disabled,
            this, &DesktopWindow::onVideoH264Disabled, Qt::QueuedConnection);

    emit sig_cursorConfig(desktop_config_.cursor_shape(), desktop_config_.cursor_position());

    clipboard_.reset(desktop_config_.clipboard() ? Clipboard::create(this) : nullptr);
    if (clipboard_)
    {
        connect(clipboard_, &Clipboard::sig_clipboardEvent, this, &DesktopWindow::onClipboardEvent);
        clipboard_->start();
    }

    worker_manager_->start();

    emit sig_startConnection(session_state_);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::reconnect()
{
    LOG(INFO) << "Reconnecting after returning to foreground";
    worker_manager_.reset();
    start();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data)
{
    if (status == NetworkWorker::Status::HOST_DISCONNECTED)
    {
        if (session_keeper_)
            session_keeper_->release();
    }

    onStatusChanged(status, data);
    if (status == NetworkWorker::Status::HOST_CONNECTED)
        onNetworkConnected();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onNetworkConnected()
{
    if (session_keeper_)
        session_keeper_->acquire();

    start_time_ = Clock::now();

    // Now the session can receive incoming messages.
    emit sig_sessionReady();
    sendCapabilities();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onStatusChanged(NetworkWorker::Status status, const QVariant& data)
{
    switch (status)
    {
        case NetworkWorker::Status::HOST_CONNECTING:
            setStatusText(tr("Connecting to host %1...").arg(session_state_->hostAddress()));
            break;

        case NetworkWorker::Status::HOST_CONNECTED:
            connected_ = true;
            was_connected_ = true;
            setStatusText(tr("Connection established."));
            break;

        case NetworkWorker::Status::HOST_DISCONNECTED:
        {
            QString message = tr("The connection to the host has been lost.");
            if (data.canConvert<TcpChannel::ErrorCode>())
                message = TcpChannel::errorToString(data.value<TcpChannel::ErrorCode>());
            setStatusText(message);
            view_->setFrame(SharedFrame());
            connected_ = false;
        }
        break;

        case NetworkWorker::Status::RELAY_ERROR:
            setStatusText(data.toString());
            break;

        case NetworkWorker::Status::VERSION_MISMATCH:
            setStatusText(tr("The host version is newer than the client. Please update the application."));
            break;

        case NetworkWorker::Status::LEGACY_HOST:
            setStatusText(tr("Legacy hosts are not supported."));
            connected_ = false;
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
void DesktopWindow::onSessionListChanged(const proto::control::SessionList& session_list)
{
    session_list_ = session_list;
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
void DesktopWindow::onCursorPositionChanged(const proto::cursor::Position& position)
{
    view_->setCursorPosition(QPointF(position.x(), position.y()));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onShowActions()
{
    action_sheet_ = new BottomSheet(this);

    int next_index = 0;
    int power_index = -1;
    int keyboard_index = -1;
    int ctrl_alt_del_index = -1;
    int users_index = -1;

    // Until the connection is established only disconnecting makes sense; the host actions all need a
    // live session.
    if (connected_)
    {
        const int screen_count = screen_list_.screen_size() > 1 ? screen_list_.screen_size() : 0;

        for (int i = 0; i < screen_count; ++i)
        {
            const bool selected = screen_list_.screen(i).id() == screen_list_.current_screen();
            action_sheet_->addItem(tr("Monitor %1").arg(i + 1), ":/img/material/monitor.svg", selected);
        }

        next_index = screen_count;

        if (power_control_available_)
        {
            power_index = next_index++;
            action_sheet_->addItem(tr("Power"), ":/img/material/power.svg");
        }

        keyboard_index = next_index++;
        action_sheet_->addItem(tr("Keyboard"), ":/img/material/keyboard.svg");

        if (host_is_windows_)
        {
            ctrl_alt_del_index = next_index++;
            action_sheet_->addItem(tr("Ctrl+Alt+Del"), ":/img/material/lock.svg");
        }

        // Only shown once the host has sent a session list (there are users to switch between).
        if (session_list_.session_size() > 0)
        {
            users_index = next_index++;
            action_sheet_->addItem(tr("Users"), ":/img/material/group.svg");
        }
    }

    const int disconnect_index = next_index;
    action_sheet_->addItem(tr("Disconnect"), ":/img/material/close.svg");

    connect(action_sheet_, &BottomSheet::sig_triggered, this,
            [this, power_index, keyboard_index, ctrl_alt_del_index, users_index, disconnect_index](int index)
    {
        if (index == power_index)
            showPowerActions();
        else if (index == users_index)
            showSessionActions();
        else if (index == keyboard_index)
            view_->showSoftwareKeyboard();
        else if (index == ctrl_alt_del_index)
            view_->sendCtrlAltDelete();
        else if (index == disconnect_index)
            emit sig_closed();
        else if (index >= 0 && index < screen_list_.screen_size())
            emit sig_screenSelected(screen_list_.screen(index));
    });

    connect(action_sheet_, &BottomSheet::sig_secretGesture, this, &DesktopWindow::onShowStatistics);

    action_sheet_->showSheet();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onShowStatistics()
{
    if (!connected_)
        return;

    // Tapping the handle closes the action sheet so the statistics dialog is shown on its own.
    if (action_sheet_)
        action_sheet_->close();

    if (statistics_dialog_)
    {
        statistics_dialog_->raise();
        return;
    }

    StatisticsDialog* dialog = new StatisticsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    statistics_dialog_ = dialog;

    connect(dialog, &StatisticsDialog::sig_metricsRequired, this, &DesktopWindow::onMetricsRequest);
    connect(network_worker_, &NetworkWorker::sig_metrics, dialog, &StatisticsDialog::onNetworkMetrics);
    connect(video_worker_, &VideoWorker::sig_metrics, dialog, &StatisticsDialog::onVideoMetrics);
    connect(audio_worker_, &AudioWorker::sig_metrics, dialog, &StatisticsDialog::onAudioMetrics);

    onMetricsRequest();
    dialog->show();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::showPowerActions()
{
    BottomSheet* sheet = new BottomSheet(this);

    // A leading back item returns to the main action sheet instead of a title.
    sheet->addItem(tr("Back"), ":/img/material/arrow_back.svg");
    sheet->addItem(tr("Shutdown"), ":/img/material/power.svg");
    sheet->addItem(tr("Reboot"), ":/img/material/restart.svg");
    if (host_is_windows_) // Safe mode is a Windows-only feature.
        sheet->addItem(tr("Safe Mode"), ":/img/material/restart.svg");
    sheet->addItem(tr("Logoff"), ":/img/material/logout.svg");
    sheet->addItem(tr("Lock"), ":/img/material/lock.svg");

    connect(sheet, &BottomSheet::sig_triggered, this, [this](int index)
    {
        // When the host is not Windows the "Safe Mode" item is absent, so the items after it shift
        // up by one; realign them with the indices below.
        if (!host_is_windows_ && index >= 3)
            ++index;

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
void DesktopWindow::showSessionActions()
{
    BottomSheet* sheet = new BottomSheet(this);

    // A leading back item returns to the main action sheet instead of a title.
    sheet->addItem(tr("Back"), ":/img/material/arrow_back.svg");

    // Map each sheet row (after the Back item) to its session id.
    QList<quint32> session_ids;
    for (int i = 0; i < session_list_.session_size(); ++i)
    {
        const proto::control::Session& session = session_list_.session(i);
        if (!isSessionVisible(session, session_list_))
            continue;

        const QString user_name = QString::fromStdString(session.user_name());
        const bool is_console = session.session_id() == session_list_.console_session_id();
        const bool current = session.session_id() == session_list_.current_session_id();

        session_ids.append(session.session_id());
        sheet->addItem(formatSessionText(i, user_name, is_console),
                       ":/img/material/person.svg", current);
    }

    connect(sheet, &BottomSheet::sig_triggered, this, [this, session_ids](int index)
    {
        if (index == 0)
        {
            onShowActions();
            return;
        }

        const int session_index = index - 1;
        if (session_index >= 0 && session_index < session_ids.size())
            emit sig_switchSession(session_ids[session_index]);
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

//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendMessage(quint8 channel_id, const QByteArray& buffer)
{
    emit sig_sendMessage(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onScreenMessage(const QByteArray& buffer)
{
    proto::screen::HostToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse screen message";
        return;
    }

    if (message.has_screen_list())
        onScreenListChanged(message.screen_list());
    else
        LOG(WARNING) << "Unhandled screen message";
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onControlMessage(const QByteArray& buffer)
{
    proto::control::HostToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse control message";
        return;
    }

    if (message.has_capabilities())
        readCapabilities(message.capabilities());
    else if (message.has_session_list())
        onSessionListChanged(message.session_list());
    else
        LOG(ERROR) << "Unhandled service message from host";
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onClipboardMessage(const QByteArray& buffer)
{
    proto::clipboard::HostToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse clipboard message";
        return;
    }

    if (message.has_event())
        readClipboardEvent(message.event());
    else
        LOG(ERROR) << "Unhandled clipboard message from host";
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onCurrentScreenChanged(const proto::screen::Screen& screen)
{
    LOG(INFO) << "Current screen changed:" << screen.id();

    proto::screen::ClientToHost message;
    message.mutable_screen()->CopyFrom(screen);
    sendMessage(proto::desktop::CHANNEL_ID_SCREEN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onPowerControl(proto::power::Control::Action action)
{
    proto::power::ClientToHost message;
    message.mutable_power_control()->set_action(action);
    sendMessage(proto::desktop::CHANNEL_ID_POWER, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onSwitchSession(quint32 session_id)
{
    proto::control::ClientToHost message;
    proto::control::SwitchSession* switch_session = message.mutable_switch_session();
    switch_session->set_session_id(session_id);

    LOG(INFO) << "Send:" << *switch_session;
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onMouseEvent(const proto::input::MouseEvent& event)
{
    proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
    message.mutable_mouse()->CopyFrom(event);
    sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                outgoing_message_.serialize<proto::input::ClientToHost>());
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onKeyEvent(const proto::input::KeyEvent& event)
{
    proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
    message.mutable_key()->CopyFrom(event);
    sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                outgoing_message_.serialize<proto::input::ClientToHost>());
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onTextEvent(const proto::input::TextEvent& event)
{
    proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
    message.mutable_text()->CopyFrom(event);
    sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                outgoing_message_.serialize<proto::input::ClientToHost>());
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onClipboardEvent(const proto::clipboard::Event& event)
{
    if (!desktop_config_.clipboard())
        return;

    ++send_clipboard_count_;

    proto::clipboard::ClientToHost message;
    message.mutable_event()->CopyFrom(event);
    sendMessage(proto::desktop::CHANNEL_ID_CLIPBOARD, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onVideoH264Disabled()
{
    h264_sw_enabled_ = false;
    sendCapabilities();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onMetricsRequest()
{
    if (!statistics_dialog_)
        return;

    // The network, video and audio rows are fed to the dialog directly by the workers; here we push
    // only the session-level counters that no worker owns.
    const std::chrono::seconds duration =
        std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start_time_);

    statistics_dialog_->setDuration(duration);
    statistics_dialog_->setClipboardMetrics(read_clipboard_count_, send_clipboard_count_);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::readCapabilities(const proto::control::Capabilities& capabilities)
{
    LOG(INFO) << "Received:" << capabilities;

    onCapabilitiesChanged(capabilities);
    sendConfig(desktop_config_);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::readClipboardEvent(const proto::clipboard::Event& event)
{
    if (desktop_config_.clipboard())
    {
        ++read_clipboard_count_;
        if (clipboard_)
            clipboard_->injectClipboardEvent(event);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendSessionListRequest()
{
    proto::control::ClientToHost message;
    proto::control::SessionsRequest* request = message.mutable_sessions_request();
    request->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendConfig(const proto::control::Config& config)
{
    LOG(INFO) << "Send:" << config;

    proto::control::ClientToHost message;
    message.mutable_config()->CopyFrom(desktop_config_);
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
    sendSessionListRequest();
}


//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendCapabilities()
{
    proto::control::ClientToHost message;
    proto::control::Capabilities* capabilities = message.mutable_capabilities();

    auto add_flag = [capabilities](const char* name, bool value)
    {
        proto::control::Capabilities::Flag* flag = capabilities->add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

    add_flag(kFlagVideoVP8, true);
    add_flag(kFlagVideoVP9, true);
    if (h264_sw_enabled_)
        add_flag(kFlagVideoH264, true);
    add_flag(kFlagAudioOpus, true);

    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

