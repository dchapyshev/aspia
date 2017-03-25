//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/screen_config.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__SCREEN_CONFIG_H
#define _ASPIA_CLIENT__SCREEN_CONFIG_H

#include "desktop_capture/pixel_format.h"
#include "proto/proto.pb.h"

namespace aspia {

static const int32_t kMaxUpdateInterval = 100;
static const int32_t kMinUpdateInterval = 15;
static const int32_t kDefUpdateInterval = 30;

static const int32_t kMaxCompressRatio = 9;
static const int32_t kMinCompressRatio = 1;
static const int32_t kDefCompressRatio = 6;


class ScreenConfig
{
public:
    ScreenConfig();
    ScreenConfig(const ScreenConfig& other);
    ~ScreenConfig() = default;

    void SetEncoding(proto::VideoEncoding encoding);
    proto::VideoEncoding Encoding() const;

    void SetFormat(const PixelFormat& format);
    const PixelFormat& Format() const;
    PixelFormat* MutableFormat();

    bool DesktopEffects() const;
    void SetDesktopEffects(bool value);

    bool CursorShape() const;
    void SetCursorShape(bool value);

    bool Clipboard() const;
    void SetClipboard(bool value);

    int32_t UpdateInterval() const;
    void SetUpdateInterval(int32_t value);

    int32_t CompressRatio() const;
    void SetCompressRatio(int32_t value);

    bool IsEqualTo(const ScreenConfig& other) const;
    void Set(const ScreenConfig& other);

    ScreenConfig& operator=(const ScreenConfig& other);

private:
    proto::VideoEncoding encoding_;
    PixelFormat format_;

    bool enable_desktop_effects_;
    bool enable_cursor_shape_;
    bool enable_clipboard_;

    int32_t update_interval_;
    int32_t compress_ratio_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__SCREEN_CONFIG_H
