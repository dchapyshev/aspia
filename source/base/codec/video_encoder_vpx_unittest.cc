//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/video_encoder_vpx.h"

#include "base/desktop/frame_aligned.h"
#include "proto/desktop_video.h"

#include <gtest/gtest.h>

#include <QRect>
#include <QSize>

#include <cstring>
#include <memory>

// Test-only access to the codec, to make libvpx fail in the middle of encode().
class VideoEncoderVpxTestPeer
{
public:
    static void dropCodec(VideoEncoderVpx& encoder) { encoder.codec_.reset(); }
};

namespace {

const QSize kFrameSize(64, 48);

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Frame> makeFrame(int pixels_value)
{
    std::unique_ptr<Frame> frame = FrameAligned::create(kFrameSize, 32);
    memset(frame->frameData(), pixels_value,
           static_cast<size_t>(frame->stride()) * static_cast<size_t>(kFrameSize.height()));
    *frame->updatedRegion() += QRect(QPoint(0, 0), kFrameSize);
    return frame;
}

//--------------------------------------------------------------------------------------------------
bool isKeyFrame(const proto::video::Packet& packet)
{
    return packet.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME;
}

//--------------------------------------------------------------------------------------------------
void checkFormatIsResentAfterTemporaryError(proto::video::Encoding encoding)
{
    VideoEncoderVpx encoder(encoding);
    std::unique_ptr<Frame> frame = makeFrame(0x40);
    proto::video::Packet packet;

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    EXPECT_TRUE(packet.has_format());
    EXPECT_TRUE(isKeyFrame(packet));

    packet.Clear();
    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    EXPECT_FALSE(packet.has_format());

    // The caller drops the packet of a failed encode(), and the format goes away with it.
    VideoEncoderVpxTestPeer::dropCodec(encoder);
    packet.Clear();
    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::TEMPORARY_ERROR);

    packet.Clear();
    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    ASSERT_TRUE(packet.has_format());
    EXPECT_EQ(packet.format().video_rect().width(), kFrameSize.width());
    EXPECT_EQ(packet.format().video_rect().height(), kFrameSize.height());
    EXPECT_TRUE(isKeyFrame(packet));
    EXPECT_EQ(packet.encoding(), encoding);
}

//--------------------------------------------------------------------------------------------------
void checkRequestedKeyFrameHasFormat(proto::video::Encoding encoding)
{
    VideoEncoderVpx encoder(encoding);
    std::unique_ptr<Frame> frame = makeFrame(0x40);
    proto::video::Packet packet;

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);

    packet.Clear();
    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    EXPECT_FALSE(packet.has_format());
    EXPECT_FALSE(isKeyFrame(packet));

    encoder.setKeyFrameRequired(true);
    packet.Clear();
    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    EXPECT_TRUE(isKeyFrame(packet));
    ASSERT_TRUE(packet.has_format());
    EXPECT_EQ(packet.format().video_rect().width(), kFrameSize.width());
    EXPECT_EQ(packet.format().video_rect().height(), kFrameSize.height());
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, Vp8FormatIsResentAfterTemporaryError)
{
    checkFormatIsResentAfterTemporaryError(proto::video::ENCODING_VP8);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, Vp9FormatIsResentAfterTemporaryError)
{
    checkFormatIsResentAfterTemporaryError(proto::video::ENCODING_VP9);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, Vp8RequestedKeyFrameHasFormat)
{
    checkRequestedKeyFrameHasFormat(proto::video::ENCODING_VP8);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, Vp9RequestedKeyFrameHasFormat)
{
    checkRequestedKeyFrameHasFormat(proto::video::ENCODING_VP9);
}
