//
// PROJECT:         Aspia
// FILE:            client/client_config.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_config.h"
#include "codec/video_helpers.h"

namespace aspia {

ClientConfig::ClientConfig()
{
    SetDefaultDesktopSessionConfig();
}

ClientConfig::ClientConfig(const ClientConfig& other)
{
    CopyFrom(other);
}

void ClientConfig::SetDefaultDesktopSessionConfig()
{
    desktop_session_config_.set_flags(
        proto::desktop::Config::ENABLE_CLIPBOARD | proto::desktop::Config::ENABLE_CURSOR_SHAPE);

    desktop_session_config_.set_video_encoding(
        proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);

    desktop_session_config_.set_update_interval(30);
    desktop_session_config_.set_compress_ratio(6);

    ConvertToVideoPixelFormat(PixelFormat::RGB565(),
                              desktop_session_config_.mutable_pixel_format());
}

void ClientConfig::CopyFrom(const ClientConfig& other)
{
    address_ = other.address_;
    port_ = other.port_;
    session_type_ = other.session_type_;

    desktop_session_config_.CopyFrom(other.desktop_session_config_);
}

void ClientConfig::set_address(const std::wstring& address)
{
    address_ = address;
}

const std::wstring& ClientConfig::address() const
{
    return address_;
}

void ClientConfig::set_session_type(proto::auth::SessionType session_type)
{
    session_type_ = session_type;
}

proto::auth::SessionType ClientConfig::session_type() const
{
    return session_type_;
}

void ClientConfig::set_port(uint16_t port)
{
    port_ = port;
}

uint16_t ClientConfig::port() const
{
    return port_;
}

const proto::desktop::Config& ClientConfig::desktop_session_config() const
{
    return desktop_session_config_;
}

proto::desktop::Config* ClientConfig::mutable_desktop_session_config()
{
    return &desktop_session_config_;
}

ClientConfig& ClientConfig::operator=(const ClientConfig& other)
{
    CopyFrom(other);
    return *this;
}

} // namespace aspia
