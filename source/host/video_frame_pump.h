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

#ifndef HOST__VIDEO_FRAME_PUMP_H
#define HOST__VIDEO_FRAME_PUMP_H

#include "base/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "desktop/desktop_region.h"
#include "net/network_listener.h"
#include "proto/desktop.pb.h"

namespace codec {
class VideoEncoder;
} // namespace codec

namespace desktop {
class Frame;
class SharedFrame;
} // namespace desktop

namespace net {
class ChannelProxy;
} // namespace net

namespace host {

class VideoFramePump : public base::SimpleThread
{
public:
    VideoFramePump(std::shared_ptr<net::ChannelProxy> channel_proxy,
                   std::unique_ptr<codec::VideoEncoder> encoder);
    ~VideoFramePump();

    void encodeFrame(const desktop::Frame& frame);

protected:
    // base::SimpleThread implementation.
    void run() override;

private:
    void reloadWorkFrame();

    std::shared_ptr<net::ChannelProxy> channel_proxy_;
    proto::HostToClient outgoing_message_;

    std::unique_ptr<desktop::SharedFrame> input_frame_;
    std::mutex input_frame_lock_;

    base::WaitableEvent work_event_;
    std::unique_ptr<desktop::SharedFrame> work_frame_;
    std::unique_ptr<codec::VideoEncoder> encoder_;

    DISALLOW_COPY_AND_ASSIGN(VideoFramePump);
};

} // namespace host

#endif // HOST__VIDEO_FRAME_PUMP_H
