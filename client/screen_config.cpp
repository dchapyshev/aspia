//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/screen_config.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/screen_config.h"

namespace aspia {

ScreenConfig::ScreenConfig() :
    encoding_(proto::VideoEncoding::VIDEO_ENCODING_ZLIB),
    disable_desktop_effects_(true),
    show_remote_cursor_(true),
    auto_send_clipboard_(false),
    update_interval_(kDefUpdateInterval),
    compress_ratio_(kDefCompressRatio),
    format_(PixelFormat::RGB565())
{
    // Nothing
}

ScreenConfig::ScreenConfig(const ScreenConfig &other)
{
    Set(other);
}

ScreenConfig::~ScreenConfig()
{
    // Nothing
}

void ScreenConfig::SetEncoding(proto::VideoEncoding encoding)
{
    encoding_ = encoding;
}

proto::VideoEncoding ScreenConfig::Encoding() const
{
    return encoding_;
}

void ScreenConfig::SetFormat(const PixelFormat &format)
{
    format_ = format;
}

const PixelFormat& ScreenConfig::Format() const
{
    return format_;
}

PixelFormat* ScreenConfig::MutableFormat()
{
    return &format_;
}

bool ScreenConfig::DisableDesktopEffects() const
{
    return disable_desktop_effects_;
}

void ScreenConfig::SetDisableDesktopEffects(bool value)
{
    disable_desktop_effects_ = value;
}

bool ScreenConfig::ShowRemoteCursor() const
{
    return show_remote_cursor_;
}

void ScreenConfig::SetShowRemoteCursor(bool value)
{
    show_remote_cursor_ = value;
}

bool ScreenConfig::AutoSendClipboard() const
{
    return auto_send_clipboard_;
}

void ScreenConfig::SetAutoSendClipboard(bool value)
{
    auto_send_clipboard_ = value;
}

int32_t ScreenConfig::UpdateInterval() const
{
    return update_interval_;
}

void ScreenConfig::SetUpdateInterval(int32_t value)
{
    update_interval_ = value;
}

int32_t ScreenConfig::CompressRatio() const
{
    return compress_ratio_;
}

void ScreenConfig::SetCompressRatio(int32_t value)
{
    compress_ratio_ = value;
}

bool ScreenConfig::IsEqualTo(const ScreenConfig &other) const
{
    return (format_.IsEqualTo(other.format_) &&
            encoding_                == other.encoding_ &&
            disable_desktop_effects_ == other.disable_desktop_effects_ &&
            show_remote_cursor_      == other.show_remote_cursor_ &&
            auto_send_clipboard_     == other.auto_send_clipboard_ &&
            update_interval_         == other.update_interval_ &&
            compress_ratio_          == other.compress_ratio_);
}

void ScreenConfig::Set(const ScreenConfig &other)
{
    encoding_                = other.encoding_;
    format_                  = other.format_;
    disable_desktop_effects_ = other.disable_desktop_effects_;
    show_remote_cursor_      = other.show_remote_cursor_;
    auto_send_clipboard_     = other.auto_send_clipboard_;
    update_interval_         = other.update_interval_;
    compress_ratio_          = other.compress_ratio_;
}

ScreenConfig& ScreenConfig::operator=(const ScreenConfig &other)
{
    Set(other);
    return *this;
}

} // namespace aspia
