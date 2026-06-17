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
#include "base/desktop/mouse_cursor.h"
#include "base/net/tcp_channel.h"
#include "client/client_desktop.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router.h"
#include "client/session_state.h"
#include "client/android/desktop_view.h"
#include "common/android/bottom_sheet.h"
#include "common/android/floating_action_button.h"
#include "common/android/label.h"
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

    start();
}

//--------------------------------------------------------------------------------------------------
DesktopWindow::~DesktopWindow()
{
    // The client lives on the IO thread; let its own event loop delete it.
    if (client_)
        client_->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (status_)
        status_->setFixedWidth(qRound(width() * kStatusWidthFactor));

    if (fab_)
        fab_->move(width() - fab_->width() - kFabMargin, height() - fab_->height() - kFabMargin);
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

    if (router->status() != Router::Status::ONLINE)
    {
        setStatusText(tr("The specified router is offline."));
        return;
    }

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
    ClientDesktop* client = new ClientDesktop(defaultDesktopConfig());

    connect(client, &ClientDesktop::sig_frameChanged,
            this, &DesktopWindow::onFrameChanged, Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_drawFrame, this,
            [this](const QList<QRect>& /* dirty_rects */) { view_->refresh(); }, Qt::QueuedConnection);
    connect(client, &Client::sig_statusChanged,
            this, &DesktopWindow::onStatusChanged, Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_mouseCursorChanged,
            view_, &DesktopView::setCursorShape, Qt::QueuedConnection);
    connect(view_, &DesktopView::sig_mouseEvent,
            client, &ClientDesktop::onMouseEvent, Qt::QueuedConnection);

    client->moveToThread(GuiApplication::ioThread());
    client->setSessionState(session_state_);
    client_ = client;

    QMetaObject::invokeMethod(client, &Client::start, Qt::QueuedConnection);
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
            setStatusText(tr("Connection established."));
            break;

        case Client::Status::HOST_DISCONNECTED:
        {
            QString message = tr("The connection to the host has been lost.");
            if (data.canConvert<TcpChannel::ErrorCode>())
                message = TcpChannel::errorToString(data.value<TcpChannel::ErrorCode>());
            setStatusText(message);
            view_->setFrame(nullptr);
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
void DesktopWindow::onFrameChanged(const QSize& /* screen_size */, std::shared_ptr<Frame> frame)
{
    view_->setFrame(std::move(frame));
    status_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onShowActions()
{
    BottomSheet* sheet = new BottomSheet(this);
    sheet->addItem(tr("Disconnect"), ":/img/material/close.svg");

    connect(sheet, &BottomSheet::sig_triggered, this, [this](int index)
    {
        if (index == 0)
            emit sig_closed();
    });

    sheet->showSheet();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::setStatusText(const QString& text)
{
    status_->setText(text);
    status_->setVisible(true);
}
