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

#include "host/host_session_desktop.h"

#include "base/clipboard.h"
#include "base/message_serialization.h"
#include "host/input_injector.h"
#include "host/screen_updater.h"

namespace aspia {

HostSessionDesktop::HostSessionDesktop(proto::auth::SessionType session_type,
                                       const QString& channel_id)
    : HostSession(channel_id),
      session_type_(session_type)
{
    switch (session_type_)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            break;

        default:
            qFatal("Invalid session type: %d", session_type_);
            break;
    }
}

void HostSessionDesktop::startSession()
{
    proto::desktop::HostToClient message;
    message.mutable_config_request()->set_dummy(1);
    emit sendMessage(serializeMessage(message));
}

void HostSessionDesktop::stopSession()
{
    delete screen_updater_;
    delete clipboard_;
    input_injector_.reset();
}

void HostSessionDesktop::messageReceived(const QByteArray& buffer)
{
    proto::desktop::ClientToHost message;

    if (!parseMessage(buffer, message))
    {
        emit errorOccurred();
        return;
    }

    if (message.has_pointer_event())
        readPointerEvent(message.pointer_event());
    else if (message.has_key_event())
        readKeyEvent(message.key_event());
    else if (message.has_clipboard_event())
        readClipboardEvent(message.clipboard_event());
    else if (message.has_config())
        readConfig(message.config());
    else if (message.has_screen())
        readScreen(message.screen());
    else
    {
        qDebug("Unhandled message from client");
    }
}

void HostSessionDesktop::clipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    proto::desktop::HostToClient message;
    message.mutable_clipboard_event()->CopyFrom(event);

    emit sendMessage(serializeMessage(message));
}

void HostSessionDesktop::readPointerEvent(const proto::desktop::PointerEvent& event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        qWarning("Attempt to inject pointer event to desktop view session");
        emit errorOccurred();
        return;
    }

    if (input_injector_.isNull())
        input_injector_.reset(new InputInjector(this));

    input_injector_->injectPointerEvent(event);
}

void HostSessionDesktop::readKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        qWarning("Attempt to inject key event to desktop view session");
        emit errorOccurred();
        return;
    }

    if (input_injector_.isNull())
        input_injector_.reset(new InputInjector(this));

    input_injector_->injectKeyEvent(event);
}

void HostSessionDesktop::readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        qWarning("Attempt to inject clipboard event to desktop view session");
        emit errorOccurred();
        return;
    }

    if (clipboard_.isNull())
    {
        qWarning("Attempt to inject clipboard event to session with the clipboard disabled");
        emit errorOccurred();
        return;
    }

    clipboard_->injectClipboardEvent(clipboard_event);
}

void HostSessionDesktop::readConfig(const proto::desktop::Config& config)
{
    delete screen_updater_;
    delete clipboard_;

    if (config.flags() & proto::desktop::ENABLE_CLIPBOARD)
    {
        if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        {
            qWarning("Attempt to enable clipboard in desktop view session");
            emit errorOccurred();
            return;
        }

        clipboard_ = new Clipboard(this);

        connect(clipboard_, &Clipboard::clipboardEvent,
                this, &HostSessionDesktop::clipboardEvent);
    }

    screen_updater_ = new ScreenUpdater(this);

    connect(screen_updater_, &ScreenUpdater::sendMessage, this, &HostSessionDesktop::sendMessage);

    if (!screen_updater_->start(config))
    {
        emit errorOccurred();
        return;
    }
}

void HostSessionDesktop::readScreen(const proto::desktop::Screen& screen)
{
    if (screen_updater_)
        screen_updater_->selectScreen(screen.id());
}

} // namespace aspia
