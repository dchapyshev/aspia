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
    ScreenConfig(const ScreenConfig &other);
    ~ScreenConfig();

    void SetEncoding(proto::VideoEncoding encoding);
    proto::VideoEncoding Encoding() const;

    void SetFormat(const PixelFormat &format);
    const PixelFormat& Format() const;
    PixelFormat* MutableFormat();

    bool DisableDesktopEffects() const;
    void SetDisableDesktopEffects(bool value);

    bool ShowRemoteCursor() const;
    void SetShowRemoteCursor(bool value);

    bool AutoSendClipboard() const;
    void SetAutoSendClipboard(bool value);

    int32_t UpdateInterval() const;
    void SetUpdateInterval(int32_t value);

    int32_t CompressRatio() const;
    void SetCompressRatio(int32_t value);

    bool IsEqualTo(const ScreenConfig &other) const;
    void Set(const ScreenConfig &other);

    ScreenConfig& operator=(const ScreenConfig &other);

private:
    proto::VideoEncoding encoding_;
    PixelFormat format_;

    bool disable_desktop_effects_;
    bool show_remote_cursor_;
    bool auto_send_clipboard_;

    int32_t update_interval_;
    int32_t compress_ratio_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__SCREEN_CONFIG_H
