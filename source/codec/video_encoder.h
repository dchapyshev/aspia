//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CODEC__VIDEO_ENCODER_H
#define CODEC__VIDEO_ENCODER_H

#include "desktop/screen_settings_tracker.h"
#include "proto/desktop.pb.h"

namespace desktop {
class Frame;
} // namespace desktop

namespace codec {

class VideoEncoder
{
public:
    virtual ~VideoEncoder() = default;

    virtual void encode(const desktop::Frame* frame, proto::VideoPacket* packet) = 0;

protected:
    void fillPacketInfo(proto::VideoEncoding encoding,
                        const desktop::Frame* frame,
                        proto::VideoPacket* packet);

private:
    desktop::ScreenSettingsTracker screen_settings_tracker_;
};

} // namespace codec

#endif // CODEC__VIDEO_ENCODER_H
