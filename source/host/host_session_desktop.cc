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

#include "base/power_controller.h"
#include "common/clipboard.h"
#include "common/desktop_session_constants.h"
#include "common/message_serialization.h"
#include "host/input_injector.h"
#include "host/host_settings.h"
#include "host/host_system_info.h"
#include "proto/desktop_session_extensions.pb.h"

#if defined(OS_WIN)
#include "desktop/win/visual_effects_disabler.h"
#include "host/win/updater_launcher.h"
#endif // defined(OS_WIN)

namespace host {

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

void HostSessionDesktop::onScreenUpdate(const QByteArray& message)
{
    sendMessage(message);
}

void HostSessionDesktop::sessionStarted()
{
    outgoing_message_.mutable_config_request()->set_dummy(1);
    sendMessage(common::serializeMessage(outgoing_message_));
}

void HostSessionDesktop::messageReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!common::parseMessage(buffer, incoming_message_))
    {
        stop();
        return;
    }

    if (incoming_message_.has_pointer_event())
        readPointerEvent(incoming_message_.pointer_event());
    else if (incoming_message_.has_key_event())
        readKeyEvent(incoming_message_.key_event());
    else if (incoming_message_.has_clipboard_event())
        readClipboardEvent(incoming_message_.clipboard_event());
    else if (incoming_message_.has_extension())
        readExtension(incoming_message_.extension());
    else if (incoming_message_.has_config())
        readConfig(incoming_message_.config());
    else
    {
        DLOG(LS_WARNING) << "Unhandled message from client";
    }
}

void HostSessionDesktop::clipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    sendMessage(common::serializeMessage(outgoing_message_));
}

void HostSessionDesktop::readPointerEvent(const proto::desktop::PointerEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_WARNING) << "Attempt to inject pointer event to desktop view session";
        stop();
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
        stop();
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
        stop();
        return;
    }

    if (!clipboard_)
    {
        LOG(LS_WARNING) << "Attempt to inject clipboard event to session with the clipboard disabled";
        stop();
        return;
    }

    clipboard_->injectClipboardEvent(clipboard_event);
}

void HostSessionDesktop::readExtension(const proto::desktop::Extension& extension)
{
    if (extension.name() == common::kSelectScreenExtension)
    {
        proto::desktop::Screen screen;

        if (!screen.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse select screen extension data";
            return;
        }

        if (screen_updater_)
            screen_updater_->selectScreen(screen.id());
    }
    else if (extension.name() == common::kPowerControlExtension)
    {
        proto::desktop::PowerControl power_control;

        if (!power_control.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse power control extension data";
            return;
        }

        switch (power_control.action())
        {
            case proto::desktop::PowerControl::ACTION_SHUTDOWN:
                base::PowerController::shutdown();
                break;

            case proto::desktop::PowerControl::ACTION_REBOOT:
                base::PowerController::reboot();
                break;

            case proto::desktop::PowerControl::ACTION_LOGOFF:
                base::PowerController::logoff();
                break;

            case proto::desktop::PowerControl::ACTION_LOCK:
                base::PowerController::lock();
                break;

            default:
                LOG(LS_WARNING) << "Unhandled power control action: " << power_control.action();
                break;
        }
    }
    else if (extension.name() == common::kRemoteUpdateExtension)
    {
        if (session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE && Settings().remoteUpdate())
        {
            launchUpdater();
        }
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        sendSystemInfo();
    }
    else
    {
        LOG(LS_WARNING) << "Unknown extension: " << extension.name();
    }
}

void HostSessionDesktop::readConfig(const proto::desktop::Config& config)
{
    uint32_t change_flags = config_tracker_.changeFlags(config);

    if (change_flags & DesktopConfigTracker::CLIPBOARD_CHANGES &&
        session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (config.flags() & proto::desktop::ENABLE_CLIPBOARD)
        {
            clipboard_.reset(new common::Clipboard());
            connect(clipboard_.get(), &common::Clipboard::clipboardEvent,
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
            effects_disabler_ = std::make_unique<desktop::VisualEffectsDisabler>();

            if (disable_effects)
                effects_disabler_->disableEffects();

            if (disable_wallpaper)
                effects_disabler_->disableWallpaper();
        }
#endif // defined(OS_WIN)
    }

    if (change_flags & DesktopConfigTracker::VIDEO_CHANGES)
    {
        screen_updater_.reset(new ScreenUpdater(this));

        if (!screen_updater_->start(config))
            stop();
    }
}

void HostSessionDesktop::sendSystemInfo()
{
    proto::system_info::SystemInfo system_info;
    createHostSystemInfo(&system_info);

    outgoing_message_.Clear();

    proto::desktop::Extension* extension = outgoing_message_.mutable_extension();
    extension->set_name(common::kSystemInfoExtension);
    extension->set_data(system_info.SerializeAsString());

    sendMessage(common::serializeMessage(outgoing_message_));
}

} // namespace host
