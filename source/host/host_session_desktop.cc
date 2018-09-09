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

#include "base/message_serialization.h"
#include "host/input_injector.h"
#include "host/screen_updater.h"
#include "share/clipboard.h"

#if defined(OS_WIN)
#include "desktop_capture/win/visual_effects_disabler.h"
#endif // defined(OS_WIN)

namespace aspia {

HostSessionDesktop::HostSessionDesktop(proto::SessionType session_type,
                                       const QString& channel_id)
    : HostSession(channel_id),
      session_type_(session_type)
{
    switch (session_type_)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
            break;

        default:
            LOG(LS_FATAL) << "Invalid session type: " << session_type_;
            break;
    }
}

HostSessionDesktop::~HostSessionDesktop()
{
#if defined(OS_WIN)
    if (effects_disabler_)
    {
        if (effects_disabler_->isEffectsDisabled())
            effects_disabler_->restoreEffects();

        if (effects_disabler_->isWallpaperDisabled())
            effects_disabler_->restoreWallpaper();
    }
#endif // defined(OS_WIN)
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
        DLOG(LS_WARNING) << "Unhandled message from client";
    }
}

void HostSessionDesktop::clipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    proto::desktop::HostToClient message;
    message.mutable_clipboard_event()->CopyFrom(event);

    emit sendMessage(serializeMessage(message));
}

void HostSessionDesktop::readPointerEvent(const proto::desktop::PointerEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_WARNING) << "Attempt to inject pointer event to desktop view session";
        emit errorOccurred();
        return;
    }

    if (input_injector_)
        input_injector_->injectPointerEvent(event);
}

void HostSessionDesktop::readKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_WARNING) << "Attempt to inject key event to desktop view session";
        emit errorOccurred();
        return;
    }

    if (input_injector_)
        input_injector_->injectKeyEvent(event);
}

void HostSessionDesktop::readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_WARNING) << "Attempt to inject clipboard event to desktop view session";
        emit errorOccurred();
        return;
    }

    if (clipboard_.isNull())
    {
        LOG(LS_WARNING) << "Attempt to inject clipboard event to session with the clipboard disabled";
        emit errorOccurred();
        return;
    }

    clipboard_->injectClipboardEvent(clipboard_event);
}

void HostSessionDesktop::readConfig(const proto::desktop::Config& config)
{
    uint32_t change_flags = config_tracker_.changeFlags(config);

    if (change_flags & DesktopConfigTracker::CLIPBOARD_CHANGES &&
        session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        delete clipboard_;

        if (config.flags() & proto::desktop::ENABLE_CLIPBOARD)
        {
            clipboard_ = new Clipboard(this);
            connect(clipboard_, &Clipboard::clipboardEvent,
                    this, &HostSessionDesktop::clipboardEvent);
        }
    }

    if (change_flags & DesktopConfigTracker::INPUT_CHANGES &&
        session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        bool block_input = config.flags() & proto::desktop::BLOCK_REMOTE_INPUT;
        input_injector_.reset(new InputInjector(this, block_input));
    }

    if (change_flags & DesktopConfigTracker::EFFECTS_CHANGES)
    {
#if defined(OS_WIN)
        bool disable_effects = config.flags() & proto::desktop::DISABLE_DESKTOP_EFFECTS;
        bool disable_wallpaper = config.flags() & proto::desktop::DISABLE_DESKTOP_WALLPAPER;

        if (effects_disabler_)
        {
            if (effects_disabler_->isEffectsDisabled())
                effects_disabler_->restoreEffects();

            if (effects_disabler_->isWallpaperDisabled())
                effects_disabler_->restoreWallpaper();
        }

        if (disable_wallpaper || disable_effects)
        {
            effects_disabler_ = std::make_unique<VisualEffectsDisabler>();

            if (disable_effects)
                effects_disabler_->disableEffects();

            if (disable_wallpaper)
                effects_disabler_->disableWallpaper();
        }
#endif // defined(OS_WIN)
    }

    if (change_flags & DesktopConfigTracker::VIDEO_CHANGES)
    {
        delete screen_updater_;
        screen_updater_ = new ScreenUpdater(this);

        connect(screen_updater_, &ScreenUpdater::sendMessage,
                this, &HostSessionDesktop::sendMessage);

        if (!screen_updater_->start(config))
            emit errorOccurred();
    }
}

void HostSessionDesktop::readScreen(const proto::desktop::Screen& screen)
{
    if (screen_updater_)
        screen_updater_->selectScreen(screen.id());
}

} // namespace aspia
