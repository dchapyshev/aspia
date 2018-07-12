//
// PROJECT:         Aspia
// FILE:            client/computer_factory.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/computer_factory.h"

#include "codec/video_util.h"

#include "build_config.h"

namespace aspia {

// static
proto::address_book::Computer ComputerFactory::defaultComputer()
{
    proto::address_book::Computer computer;

    computer.set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
    computer.set_port(kDefaultHostTcpPort);

    setDefaultDesktopManageConfig(computer.mutable_session_config()->mutable_desktop_manage());
    setDefaultDesktopViewConfig(computer.mutable_session_config()->mutable_desktop_view());

    return computer;
}

// static
proto::desktop::Config ComputerFactory::defaultDesktopManageConfig()
{
    proto::desktop::Config config;
    setDefaultDesktopManageConfig(&config);
    return config;
}

// static
proto::desktop::Config ComputerFactory::defaultDesktopViewConfig()
{
    proto::desktop::Config config;
    setDefaultDesktopViewConfig(&config);
    return config;
}

// static
void ComputerFactory::setDefaultDesktopManageConfig(proto::desktop::Config* config)
{
    Q_ASSERT(config);

    config->set_features(proto::desktop::FEATURE_CLIPBOARD | proto::desktop::FEATURE_CURSOR_SHAPE);
    config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    config->set_update_interval(30);
    config->set_compress_ratio(6);

    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), config->mutable_pixel_format());
}

// static
void ComputerFactory::setDefaultDesktopViewConfig(proto::desktop::Config* config)
{
    Q_ASSERT(config);

    config->set_features(0);
    config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    config->set_update_interval(30);
    config->set_compress_ratio(6);

    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), config->mutable_pixel_format());
}

} // namespace aspia
