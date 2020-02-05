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

#include "host/desktop_session_fake.h"

#include "base/logging.h"
#include "desktop/desktop_frame_simple.h"
#include "desktop/shared_desktop_frame.h"
#include "proto/desktop.pb.h"

namespace host {

namespace {

const int kFrameWidth = 800;
const int kFrameHeight = 600;
const int kBoxWidth = 50;
const int kBoxHeight = 50;

static_assert(kBoxWidth < kFrameWidth && kBoxHeight < kFrameHeight);
static_assert((kFrameWidth % kBoxWidth) == 0 && (kFrameHeight % kBoxHeight) == 0);

} // namespace

DesktopSessionFake::DesktopSessionFake(Delegate* delegate)
    : box_speed_x_(kBoxWidth),
      box_speed_y_(kBoxHeight),
      delegate_(delegate)
{
    DCHECK(delegate_);
}

DesktopSessionFake::~DesktopSessionFake() = default;

void DesktopSessionFake::start()
{
    std::unique_ptr<desktop::Frame> frame = desktop::FrameSimple::create(
        desktop::Size(kFrameWidth, kFrameHeight), desktop::PixelFormat::ARGB());

    memset(frame->frameData(), 0, frame->stride() * frame->size().height());

    frame_ = desktop::SharedFrame::wrap(std::move(frame));
}

void DesktopSessionFake::startSession()
{
    int old_box_pos_x = box_pos_x_;
    int old_box_pos_y = box_pos_y_;

    if (box_pos_x_ + kBoxWidth >= kFrameWidth || box_pos_x_ == 0)
        box_speed_x_ = -box_speed_x_;

    if (box_pos_y_ + kBoxHeight >= kFrameHeight || box_pos_y_ == 0)
        box_speed_y_ = -box_speed_y_;

    box_pos_x_ += box_speed_x_;
    box_pos_y_ += box_speed_y_;

    // Clear previous box position.
    for (int y = 0; y < kBoxHeight; ++y)
    {
        uint8_t* data = frame_->frameDataAtPos(old_box_pos_x, old_box_pos_y);
        memset(data, 0, kBoxWidth * frame_->format().bytesPerPixel());
    }

    // Draw the box in a new position.
    for (int y = 0; y < kBoxHeight; ++y)
    {
        uint32_t* data = reinterpret_cast<uint32_t*>(
            frame_->frameDataAtPos(box_pos_x_, box_pos_y_));

        for (int x = 0; x < kBoxWidth; ++x)
            data[x] = color_queue_[color_index_];
    }

    desktop::Region* updated_region = frame_->updatedRegion();

    updated_region->clear();
    updated_region->addRect(
        desktop::Rect::makeXYWH(old_box_pos_x, old_box_pos_y, kBoxWidth, kBoxHeight));
    updated_region->addRect(
        desktop::Rect::makeXYWH(box_pos_x_, box_pos_y_, kBoxWidth, kBoxHeight));

    delegate_->onScreenCaptured(*frame_);
}

void DesktopSessionFake::stopSession()
{
    // TODO
}

void DesktopSessionFake::selectScreen(const proto::Screen& /* screen */)
{
    // Nothing
}

void DesktopSessionFake::setFeatures(const proto::internal::SetFeatures& /* features */)
{
    // Nothing
}

void DesktopSessionFake::injectKeyEvent(const proto::KeyEvent& event)
{
    // When release the button, change the color of the box.
    if (!(event.flags() & proto::KeyEvent::PRESSED))
        nextBoxColor();
}

void DesktopSessionFake::injectPointerEvent(const proto::PointerEvent& event)
{
    if (event.mask() == prev_pointer_mask_)
        return;

    // When click the mouse button, change the color of the box.
    nextBoxColor();

    prev_pointer_mask_ = event.mask();
}

void DesktopSessionFake::injectClipboardEvent(const proto::ClipboardEvent& /* event */)
{
    // Nothing. Ignore the event.
}

void DesktopSessionFake::logoffUserSession()
{
    // Nothing
}

void DesktopSessionFake::lockUserSession()
{
    // Nothing
}

void DesktopSessionFake::nextBoxColor()
{
    color_index_ = (color_index_ + 1) % kQueueLength;
}

} // namespace host
