//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/video_frame_pump.h"

#include "base/logging.h"
#include "codec/video_encoder.h"
#include "common/message_serialization.h"
#include "desktop/desktop_frame_aligned.h"
#include "desktop/shared_desktop_frame.h"
#include "net/network_channel_proxy.h"

namespace host {

VideoFramePump::VideoFramePump(std::shared_ptr<net::ChannelProxy> channel_proxy,
                               std::unique_ptr<codec::VideoEncoder> encoder)
    : channel_proxy_(std::move(channel_proxy)),
      work_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                  base::WaitableEvent::InitialState::NOT_SIGNALED),
      encoder_(std::move(encoder))
{
    DCHECK(channel_proxy_);
    DCHECK(encoder_);
}

VideoFramePump::~VideoFramePump()
{
    work_event_.signal();
    stop();
}

void VideoFramePump::encodeFrame(const desktop::Frame& frame)
{
    std::scoped_lock lock(input_frame_lock_);

    if (frame.size() != input_frame_->size() || frame.format() != input_frame_->format())
    {
        std::unique_ptr<desktop::Frame> input_frame =
            desktop::FrameAligned::create(frame.size(), frame.format(), 32);

        input_frame_ = desktop::SharedFrame::wrap(std::move(input_frame));

        desktop::Rect frame_rect = desktop::Rect::makeSize(input_frame_->size());

        // The frame is completely replaced. Add the entire frame to the changed area.
        input_frame_->updatedRegion()->addRect(frame_rect);
        input_frame_->copyPixelsFrom(frame, desktop::Point(0, 0), frame_rect);
    }
    else
    {
        for (desktop::Region::Iterator it(frame.constUpdatedRegion()); !it.isAtEnd(); it.advance())
        {
            const desktop::Rect& rect = it.rect();

            input_frame_->copyPixelsFrom(frame, rect.topLeft(), rect);
            input_frame_->updatedRegion()->addRect(rect);
        }
    }

    work_event_.signal();
}

void VideoFramePump::run()
{
    while (!isStopping())
    {
        reloadWorkFrame();

        if (!work_frame_->constUpdatedRegion().isEmpty())
        {
            outgoing_message_.Clear();

            // Encode the frame into a video packet.
            encoder_->encode(work_frame_.get(), outgoing_message_.mutable_video_packet());

            channel_proxy_->send(common::serializeMessage(outgoing_message_));

            // Clear the region.
            work_frame_->updatedRegion()->clear();
        }

        work_event_.wait();
    }
}

void VideoFramePump::reloadWorkFrame()
{
    std::scoped_lock lock(input_frame_lock_);

    if (input_frame_->constUpdatedRegion().isEmpty())
        return;

    if (!input_frame_->shareFrameWith(*work_frame_))
        work_frame_ = input_frame_->share();

    work_frame_->copyFrameInfoFrom(*input_frame_);
}

} // namespace host
