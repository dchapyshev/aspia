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
    enable_desktop_effects_(false),
    enable_cursor_shape_(true),
    enable_clipboard_(true),
    update_interval_(kDefUpdateInterval),
    compress_ratio_(kDefCompressRatio),
    format_(PixelFormat::RGB565())
{
    // Nothing
}

ScreenConfig::ScreenConfig(const ScreenConfig& other)
{
    Set(other);
}

void ScreenConfig::SetEncoding(proto::VideoEncoding encoding)
{
    encoding_ = encoding;
}

proto::VideoEncoding ScreenConfig::Encoding() const
{
    return encoding_;
}

void ScreenConfig::SetFormat(const PixelFormat& format)
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

bool ScreenConfig::DesktopEffects() const
{
    return enable_desktop_effects_;
}

void ScreenConfig::SetDesktopEffects(bool value)
{
    enable_desktop_effects_ = value;
}

bool ScreenConfig::CursorShape() const
{
    return enable_cursor_shape_;
}

void ScreenConfig::SetCursorShape(bool value)
{
    enable_cursor_shape_ = value;
}

bool ScreenConfig::Clipboard() const
{
    return enable_clipboard_;
}

void ScreenConfig::SetClipboard(bool value)
{
    enable_clipboard_ = value;
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

bool ScreenConfig::IsEqualTo(const ScreenConfig& other) const
{
    return (format_.IsEqual(other.format_) &&
            encoding_               == other.encoding_ &&
            enable_desktop_effects_ == other.enable_desktop_effects_ &&
            enable_cursor_shape_    == other.enable_cursor_shape_ &&
            enable_clipboard_       == other.enable_clipboard_ &&
            update_interval_        == other.update_interval_ &&
            compress_ratio_         == other.compress_ratio_);
}

void ScreenConfig::Set(const ScreenConfig& other)
{
    encoding_               = other.encoding_;
    format_                 = other.format_;
    enable_desktop_effects_ = other.enable_desktop_effects_;
    enable_cursor_shape_    = other.enable_cursor_shape_;
    enable_clipboard_       = other.enable_clipboard_;
    update_interval_        = other.update_interval_;
    compress_ratio_         = other.compress_ratio_;
}

ScreenConfig& ScreenConfig::operator=(const ScreenConfig& other)
{
    Set(other);
    return *this;
}

} // namespace aspia
