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
#include "host/input_thread.h"
#include "host/host_system_info.h"
#include "proto/desktop_extensions.pb.h"
#if defined(OS_WIN)
#include "host/win/updater_launcher.h"
#endif // defined(OS_WIN)

namespace host {

SessionDesktop::SessionDesktop(proto::SessionType session_type, const QString& channel_id)
    : Session(channel_id),
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

SessionDesktop::~SessionDesktop() = default;

void SessionDesktop::onScreenUpdate(const QByteArray& message)
{
    sendMessage(message);
}

void SessionDesktop::sessionStarted()
{
    const char* extensions;

    // Supported extensions are different for managing and viewing the desktop.
    if (session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE)
        extensions = common::kSupportedExtensionsForManage;
    else
        extensions = common::kSupportedExtensionsForView;

    // Add supported extensions to the list.
    extensions_ = QString::fromLatin1(extensions).split(QLatin1Char(';'));

    // Create a configuration request.
    proto::desktop::ConfigRequest* request = outgoing_message_.mutable_config_request();

    // Add supported extensions and video encodings.
    request->set_extensions(extensions);
    request->set_video_encodings(common::kSupportedVideoEncodings);

    // Send the request.
    sendMessage(common::serializeMessage(outgoing_message_));
}

void SessionDesktop::messageReceived(const QByteArray& buffer)
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

void SessionDesktop::clipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    sendMessage(common::serializeMessage(outgoing_message_));
}

void SessionDesktop::readPointerEvent(const proto::desktop::PointerEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_WARNING) << "Attempt to inject pointer event to desktop view session";
        stop();
        return;
    }

    if (input_thread_)
        input_thread_->injectPointerEvent(event);
}

void SessionDesktop::readKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_WARNING) << "Attempt to inject key event to desktop view session";
        stop();
        return;
    }

    if (input_thread_)
        input_thread_->injectKeyEvent(event);
}

void SessionDesktop::readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event)
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

void SessionDesktop::readExtension(const proto::desktop::Extension& extension)
{
    if (!extensions_.contains(QString::fromStdString(extension.name())))
    {
        DLOG(LS_WARNING) << "Unsupported or disabled extensions: " << extension.name();
        return;
    }

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
        launchUpdater();
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

void SessionDesktop::readConfig(const proto::desktop::Config& config)
{
    uint32_t mask = config_tracker_.changesMask(config);

    if (session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (mask & DesktopConfigTracker::HAS_CLIPBOARD)
        {
            if (config.flags() & proto::desktop::ENABLE_CLIPBOARD)
            {
                clipboard_.reset(new common::Clipboard());
                connect(clipboard_.get(), &common::Clipboard::clipboardEvent,
                        this, &SessionDesktop::clipboardEvent);
            }
            else
            {
                clipboard_.reset();
            }
        }

        if (mask & DesktopConfigTracker::HAS_INPUT)
        {
            bool block_input = config.flags() & proto::desktop::BLOCK_REMOTE_INPUT;
            input_thread_.reset(new InputThread(this, block_input));
        }
    }

    if (mask & DesktopConfigTracker::HAS_VIDEO)
    {
        screen_updater_.reset(new ScreenUpdater(this));

        if (!screen_updater_->start(config))
            stop();
    }
}

void SessionDesktop::sendSystemInfo()
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
