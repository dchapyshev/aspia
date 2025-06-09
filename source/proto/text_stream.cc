//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "proto/text_stream.h"

namespace {

const char* routerStateToString(proto::internal::RouterState::State state)
{
    switch (state)
    {
        case proto::internal::RouterState::DISABLED:
            return "DISABLED";

        case proto::internal::RouterState::CONNECTING:
            return "CONNECTING";

        case proto::internal::RouterState::CONNECTED:
            return "CONNECTED";

        case proto::internal::RouterState::FAILED:
            return "FAILED";

        default:
            return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
const char* routerSessionTypeToString(proto::RouterSession session_type)
{
    switch (session_type)
    {
        case proto::ROUTER_SESSION_CLIENT:
            return "ROUTER_SESSION_CLIENT";

        case proto::ROUTER_SESSION_HOST:
            return "ROUTER_SESSION_HOST";

        case proto::ROUTER_SESSION_ADMIN:
            return "ROUTER_SESSION_ADMIN";

        case proto::ROUTER_SESSION_RELAY:
            return "ROUTER_SESSION_RELAY";

        default:
            return "ROUTER_SESSION_UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
const char* controlActionToString(proto::internal::DesktopControl::Action action)
{
    switch (action)
    {
        case proto::internal::DesktopControl::ENABLE:
            return "ENABLE";

        case proto::internal::DesktopControl::DISABLE:
            return "DISABLE";

        case proto::internal::DesktopControl::LOCK:
            return "LOCK";

        case proto::internal::DesktopControl::LOGOFF:
            return "LOGOFF";

        default:
            return "Unknown control action";
    }
}

//--------------------------------------------------------------------------------------------------
const char* audioEncodingToString(proto::desktop::AudioEncoding encoding)
{
    switch (encoding)
    {
        case proto::desktop::AUDIO_ENCODING_OPUS:
            return "AUDIO_ENCODING_OPUS";

        case proto::desktop::AUDIO_ENCODING_RAW:
            return "AUDIO_ENCODING_RAW";

        default:
            return "AUDIO_ENCODING_UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
const char* videoEncodingToString(proto::desktop::VideoEncoding encoding)
{
    switch (encoding)
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            return "VIDEO_ENCODING_VP8";

        case proto::desktop::VIDEO_ENCODING_VP9:
            return "VIDEO_ENCODING_VP9";

        case proto::desktop::VIDEO_ENCODING_ZSTD:
            return "VIDEO_ENCODING_ZSTD";

        default:
            return "VIDEO_ENCODING_UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
const char* videoErrorCodeToString(proto::desktop::VideoErrorCode error_code)
{
    switch (error_code)
    {
        case proto::desktop::VIDEO_ERROR_CODE_OK:
            return "VIDEO_ERROR_CODE_OK";

        case proto::desktop::VIDEO_ERROR_CODE_TEMPORARY:
            return "VIDEO_ERROR_CODE_TEMPORARY";

        case proto::desktop::VIDEO_ERROR_CODE_PERMANENT:
            return "VIDEO_ERROR_CODE_PERMANENT";

        case proto::desktop::VIDEO_ERROR_CODE_PAUSED:
            return "VIDEO_ERROR_CODE_PAUSED";

        default:
            return "VIDEO_ERROR_CODE_UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
const char* sessionTypeToString(proto::peer::SessionType session_type)
{
    switch (session_type)
    {
         case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
            return "SESSION_TYPE_DESKTOP_MANAGE";

        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
            return "SESSION_TYPE_DESKTOP_VIEW";

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            return "SESSION_TYPE_FILE_TRANSFER";

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            return "SESSION_TYPE_SYSTEM_INFO";

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            return "SESSION_TYPE_TEXT_CHAT";

        case proto::peer::SESSION_TYPE_PORT_FORWARDING:
            return "SESSION_TYPE_PORT_FORWARDING";

        default:
            return "UNKNOWN";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& out, const proto::internal::RouterState& state)
{
    return out << "RouterState(" << QString::fromStdString(state.host_name()) << ":"
               << state.host_port() << " " << routerStateToString(state.state()) << ")";
}

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& out, proto::RouterSession session_type)
{
    return out << routerSessionTypeToString(session_type);
}

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& out, proto::internal::DesktopControl::Action action)
{
    return out << controlActionToString(action);
}

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& out, proto::desktop::AudioEncoding encoding)
{
    return out << audioEncodingToString(encoding);
}

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& out, proto::desktop::VideoEncoding encoding)
{
    return out << videoEncodingToString(encoding);
}

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& out, proto::desktop::VideoErrorCode error_code)
{
    return out << videoErrorCodeToString(error_code);
}

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& out, proto::peer::SessionType session_type)
{
    return out << sessionTypeToString(session_type);
}
