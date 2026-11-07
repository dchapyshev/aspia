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

#include "client/workers/video_worker.h"

#include "base/codec/video_decoder.h"
#include "base/codec/video_encoder_vpx.h"
#include "base/codec/webm_video_encoder.h"
#include "base/desktop/frame_aligned.h"

#include <gtest/gtest.h>

#include <QRect>
#include <QSize>

#include <cstring>
#include <memory>

// Test-only access to the packet decoding, the packets are handed in already parsed.
class VideoWorkerTestPeer
{
public:
    static void decodePacket(VideoWorker& worker, const proto::video::Packet& packet)
    {
        worker.decodePacket(packet);
    }
};

namespace {

const QSize kFrameSize(64, 48);

struct EncodedStream
{
    proto::video::Packet key_frame;
    proto::video::Packet delta_frame;
    proto::video::Packet requested_key_frame;
};

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
EncodedStream encodeStream()
{
    VideoEncoderVpx encoder;
    EncodedStream stream;

    std::unique_ptr<Frame> first = makeFrame(0x40);
    EXPECT_EQ(encoder.encode(first.get(), &stream.key_frame), VideoEncoder::Result::SUCCESS);
    EXPECT_TRUE(stream.key_frame.has_format());

    std::unique_ptr<Frame> second = makeFrame(0x80);
    EXPECT_EQ(encoder.encode(second.get(), &stream.delta_frame), VideoEncoder::Result::SUCCESS);
    EXPECT_FALSE(stream.delta_frame.has_format());
    EXPECT_FALSE(stream.delta_frame.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME);

    encoder.setKeyFrameRequired(true);
    std::unique_ptr<Frame> third = makeFrame(0xC0);
    EXPECT_EQ(encoder.encode(third.get(), &stream.requested_key_frame),
              VideoEncoder::Result::SUCCESS);
    EXPECT_TRUE(stream.requested_key_frame.has_format());
    EXPECT_TRUE(stream.requested_key_frame.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME);

    return stream;
}

//--------------------------------------------------------------------------------------------------
// Hosts of older versions may still send VP8. Here only the encoder of recordings makes it: it takes
// decoded frames and gives a key frame only when it starts, a second encoder makes the requested one.
EncodedStream encodeVp8Stream()
{
    const EncodedStream source = encodeStream();
    std::unique_ptr<VideoDecoder> decoder = VideoDecoder::create(proto::video::ENCODING_VP9);
    EncodedStream stream;

    WebmVideoEncoder encoder;
    EXPECT_EQ(decoder->decode(source.key_frame), VideoDecoder::Result::SUCCESS);
    EXPECT_TRUE(encoder.encode(decoder->frame(), &stream.key_frame));
    EXPECT_TRUE(stream.key_frame.has_format());

    EXPECT_EQ(decoder->decode(source.delta_frame), VideoDecoder::Result::SUCCESS);
    EXPECT_TRUE(encoder.encode(decoder->frame(), &stream.delta_frame));
    EXPECT_FALSE(stream.delta_frame.has_format());

    WebmVideoEncoder second_encoder;
    EXPECT_EQ(decoder->decode(source.requested_key_frame), VideoDecoder::Result::SUCCESS);
    EXPECT_TRUE(second_encoder.encode(decoder->frame(), &stream.requested_key_frame));
    EXPECT_TRUE(stream.requested_key_frame.has_format());

    return stream;
}

//--------------------------------------------------------------------------------------------------
proto::video::Packet withScreenSize(const proto::video::Packet& packet, const QSize& screen_size)
{
    proto::video::Packet result = packet;
    proto::video::Size* size = result.mutable_format()->mutable_screen_size();
    size->set_width(screen_size.width());
    size->set_height(screen_size.height());
    return result;
}

//--------------------------------------------------------------------------------------------------
class VideoWorkerTest : public testing::Test
{
protected:
    VideoWorkerTest()
    {
        // The worker belongs to its own thread that never starts here, so the signals are taken
        // directly in the test thread.
        QObject::connect(&worker_, &VideoWorker::sig_drawFrame, &worker_,
                         [this]() { ++draw_count_; }, Qt::DirectConnection);
        QObject::connect(&worker_, &VideoWorker::sig_sendMessage, &worker_,
                         [this]() { ++send_count_; }, Qt::DirectConnection);
        QObject::connect(&worker_, &VideoWorker::sig_frameChanged, &worker_,
                         [this](const QSize& screen_size, SharedFrame /* frame */)
        {
            ++frame_changed_count_;
            reported_screen_size_ = screen_size;
        }, Qt::DirectConnection);
    }

    void decode(const proto::video::Packet& packet)
    {
        VideoWorkerTestPeer::decodePacket(worker_, packet);
    }

    VideoWorker worker_;
    int draw_count_ = 0;
    int send_count_ = 0;
    int frame_changed_count_ = 0;
    QSize reported_screen_size_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
TEST_F(VideoWorkerTest, LatePacketOfPreviousEncoderIsIgnored)
{
    const EncodedStream vp8 = encodeVp8Stream();
    const EncodedStream vp9 = encodeStream();

    // After the switch from TCP to UDP the host already sends VP8, while a VP9 packet sent earlier
    // over TCP arrives between them.
    decode(vp8.key_frame);
    decode(vp9.delta_frame);
    decode(vp8.delta_frame);

    EXPECT_EQ(draw_count_, 2);
    EXPECT_EQ(send_count_, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(VideoWorkerTest, NewEncoderSwitchesDecoder)
{
    const EncodedStream vp8 = encodeVp8Stream();
    const EncodedStream vp9 = encodeStream();

    decode(vp8.key_frame);
    decode(vp9.key_frame);
    decode(vp9.delta_frame);

    EXPECT_EQ(draw_count_, 3);
    EXPECT_EQ(send_count_, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(VideoWorkerTest, LateKeyFrameOfPreviousEncoderDoesNotFreezeVideo)
{
    const EncodedStream initial = encodeVp8Stream();
    const EncodedStream tcp = encodeStream();
    const EncodedStream udp = encodeVp8Stream();

    // The host switched to VP9 over TCP shortly before it moved to UDP and to VP8. The first VP9
    // packets were still on the way over TCP and arrive after the key frame of the new VP8 encoder.
    decode(initial.key_frame);
    decode(udp.key_frame);
    decode(tcp.key_frame);
    decode(tcp.delta_frame);

    const int draw_count = draw_count_;

    decode(udp.delta_frame);
    EXPECT_EQ(send_count_, 0);

    for (int i = 0; i < 100 && send_count_ == 0; ++i)
        decode(udp.delta_frame);
    EXPECT_EQ(send_count_, 1);
    EXPECT_EQ(draw_count_, draw_count);

    // The requested key frame is still on the way.
    for (int i = 0; i < 100; ++i)
        decode(udp.delta_frame);
    EXPECT_EQ(send_count_, 1);

    decode(udp.requested_key_frame);
    EXPECT_EQ(draw_count_, draw_count + 1);
    EXPECT_EQ(send_count_, 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(VideoWorkerTest, NewScreenSizeOfSameFrameSizeIsReported)
{
    const EncodedStream stream = encodeStream();

    decode(withScreenSize(stream.key_frame, QSize(1920, 1080)));
    EXPECT_EQ(frame_changed_count_, 1);
    EXPECT_EQ(reported_screen_size_, QSize(1920, 1080));

    decode(stream.delta_frame);
    EXPECT_EQ(frame_changed_count_, 1);

    decode(withScreenSize(stream.requested_key_frame, QSize(1280, 720)));
    EXPECT_EQ(frame_changed_count_, 2);
    EXPECT_EQ(reported_screen_size_, QSize(1280, 720));
}

//--------------------------------------------------------------------------------------------------
TEST_F(VideoWorkerTest, SameScreenSizeIsNotReportedAgain)
{
    const EncodedStream stream = encodeStream();

    decode(withScreenSize(stream.key_frame, QSize(1920, 1080)));
    decode(withScreenSize(stream.requested_key_frame, QSize(1920, 1080)));
    EXPECT_EQ(frame_changed_count_, 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(VideoWorkerTest, PacketsBeforeFirstFormatDoNotRequestKeyFrame)
{
    const EncodedStream stream = encodeStream();

    // A client that joins a running session gets the packets of the current encoder until the host
    // creates a new encoder for its configuration.
    for (int i = 0; i < 100; ++i)
        decode(stream.delta_frame);
    EXPECT_EQ(send_count_, 0);
    EXPECT_EQ(draw_count_, 0);

    decode(stream.key_frame);
    EXPECT_EQ(draw_count_, 1);
    EXPECT_EQ(send_count_, 0);
}
