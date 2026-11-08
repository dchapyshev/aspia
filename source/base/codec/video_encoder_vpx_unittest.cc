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

#include "base/codec/video_decoder.h"
#include "base/codec/webm_video_encoder.h"
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
    static quint32 minQuantizer(const VideoEncoderVpx& encoder) { return encoder.min_quantizer_; }
    static quint32 maxQuantizer(const VideoEncoderVpx& encoder) { return encoder.max_quantizer_; }
    static void setTopOffQuantizer(VideoEncoderVpx& encoder, quint32 quantizer)
    {
        encoder.top_off_quantizer_ = quantizer;
    }
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
// Noise does not fit into the budget of a narrow link, so the key frame comes out above the floor.
std::unique_ptr<Frame> makeNoiseFrame(const QSize& size)
{
    std::unique_ptr<Frame> frame = FrameAligned::create(size, 32);
    *frame->updatedRegion() += QRect(QPoint(0, 0), size);
    quint32 seed = 1;

    for (int y = 0; y < size.height(); ++y)
    {
        quint8* row = frame->frameData() + y * frame->stride();
        for (int x = 0; x < size.width() * 4; ++x)
        {
            seed = seed * 1664525 + 1013904223;
            row[x] = static_cast<quint8>(seed >> 24);
        }
    }

    return frame;
}

//--------------------------------------------------------------------------------------------------
void changeRect(Frame* frame, const QRect& rect)
{
    for (int y = rect.top(); y <= rect.bottom(); ++y)
    {
        quint32* row = reinterpret_cast<quint32*>(frame->frameDataAtPos(rect.left(), y));
        for (int x = 0; x < rect.width(); ++x)
            row[x] ^= 0x00FFFFFF;
    }

    *frame->updatedRegion() = Region(rect);
}

//--------------------------------------------------------------------------------------------------
qint64 dirtyArea(const proto::video::Packet& packet)
{
    qint64 area = 0;
    for (const proto::video::Rect& rect : packet.dirty_rect())
        area += static_cast<qint64>(rect.width()) * rect.height();
    return area;
}

//--------------------------------------------------------------------------------------------------
bool isKeyFrame(const proto::video::Packet& packet)
{
    return packet.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME;
}

//--------------------------------------------------------------------------------------------------
int encodeTopOff(VideoEncoderVpx& encoder, Frame* frame)
{
    const int kMaxFrames = 240;

    *frame->updatedRegion() = Region();

    for (int i = 0; i < kMaxFrames; ++i)
    {
        if (!encoder.isTopOffRequired())
            return i;

        proto::video::Packet packet;
        EXPECT_EQ(encoder.encode(frame, &packet), VideoEncoder::Result::SUCCESS);
        EXPECT_FALSE(isKeyFrame(packet));
        EXPECT_GT(packet.dirty_rect_size(), 0);
    }

    return kMaxFrames;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, QualityKeepsTheQuantizerAtAnyBandwidth)
{
    VideoEncoderVpx encoder;
    encoder.setQuality(5);

    for (qint64 bandwidth : { qint64(0), qint64(100 * 1024), qint64(10 * 1024 * 1024) })
    {
        encoder.setBandwidth(bandwidth);
        EXPECT_EQ(VideoEncoderVpxTestPeer::minQuantizer(encoder), 32u);
        EXPECT_EQ(VideoEncoderVpxTestPeer::maxQuantizer(encoder), 32u);
    }
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, QualityEndsMatchTheBandwidthRange)
{
    VideoEncoderVpx encoder;

    encoder.setQuality(1);
    encoder.setBandwidth(0);
    EXPECT_EQ(VideoEncoderVpxTestPeer::maxQuantizer(encoder), 50u);

    encoder.setQuality(10);
    encoder.setBandwidth(0);
    EXPECT_EQ(VideoEncoderVpxTestPeer::minQuantizer(encoder), 8u);

    encoder.setQuality(11);
    encoder.setBandwidth(0);
    EXPECT_EQ(VideoEncoderVpxTestPeer::minQuantizer(encoder), 8u);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, AutoQualityFollowsTheBandwidth)
{
    VideoEncoderVpx encoder;
    encoder.setQuality(0);

    encoder.setBandwidth(100 * 1024);
    EXPECT_EQ(VideoEncoderVpxTestPeer::minQuantizer(encoder), 16u);
    EXPECT_EQ(VideoEncoderVpxTestPeer::maxQuantizer(encoder), 50u);

    encoder.setBandwidth(10 * 1024 * 1024);
    EXPECT_EQ(VideoEncoderVpxTestPeer::minQuantizer(encoder), 8u);
    EXPECT_EQ(VideoEncoderVpxTestPeer::maxQuantizer(encoder), 25u);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, FormatIsResentAfterTemporaryError)
{
    VideoEncoderVpx encoder;
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
    EXPECT_EQ(packet.encoding(), proto::video::ENCODING_VP9);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, RequestedKeyFrameHasFormat)
{
    VideoEncoderVpx encoder;
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

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, TopOffImprovesAStaticPicture)
{
    VideoEncoderVpx encoder;
    encoder.setBandwidth(8 * 1024);

    std::unique_ptr<Frame> frame = makeNoiseFrame(QSize(256, 192));
    proto::video::Packet packet;

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    ASSERT_TRUE(encoder.isTopOffRequired());

    const int frames = encodeTopOff(encoder, frame.get());
    EXPECT_GT(frames, 0);
    EXPECT_LT(frames, 240);
    EXPECT_FALSE(encoder.isTopOffRequired());
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, TopOffStopsWhenTheQuantizerStalls)
{
    const int kFrameRate = 10;
    const QSize kNoiseSize(256, 192);

    VideoEncoderVpx reference;
    reference.setFrameRate(kFrameRate);
    reference.setBandwidth(8 * 1024);

    std::unique_ptr<Frame> reference_frame = makeNoiseFrame(kNoiseSize);
    proto::video::Packet packet;

    ASSERT_EQ(reference.encode(reference_frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    const int reference_frames = encodeTopOff(reference, reference_frame.get());

    // The frames that do not reach the floor do not improve on a quantizer just above it.
    VideoEncoderVpx encoder;
    encoder.setFrameRate(kFrameRate);
    encoder.setBandwidth(8 * 1024);

    std::unique_ptr<Frame> frame = makeNoiseFrame(kNoiseSize);
    packet.Clear();

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    VideoEncoderVpxTestPeer::setTopOffQuantizer(encoder, VideoEncoderVpxTestPeer::minQuantizer(encoder) + 1);

    const int frames = encodeTopOff(encoder, frame.get());
    EXPECT_GE(frames, kFrameRate);
    EXPECT_LT(frames, reference_frames);
    EXPECT_FALSE(encoder.isTopOffRequired());
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, TopOffGoesWithSmallChanges)
{
    const QSize kNoiseSize(256, 192);
    const int kMaxFrames = 240;

    VideoEncoderVpx encoder;
    encoder.setBandwidth(8 * 1024);

    std::unique_ptr<Frame> frame = makeNoiseFrame(kNoiseSize);
    proto::video::Packet packet;

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    ASSERT_TRUE(encoder.isTopOffRequired());

    int frames = 0;
    for (; frames < kMaxFrames && encoder.isTopOffRequired(); ++frames)
    {
        changeRect(frame.get(), QRect(64, 64, 16, 16));

        packet.Clear();
        ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);

        if (frames == 0)
            EXPECT_EQ(dirtyArea(packet), kNoiseSize.width() * kNoiseSize.height());
    }

    EXPECT_LT(frames, kMaxFrames);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, TopOffWaitsWhileTheScreenIsBusy)
{
    const QSize kNoiseSize(256, 192);
    const QRect kChange(QPoint(0, 0), QSize(kNoiseSize.width() / 2, kNoiseSize.height()));

    VideoEncoderVpx encoder;
    encoder.setBandwidth(8 * 1024);

    std::unique_ptr<Frame> frame = makeNoiseFrame(kNoiseSize);
    proto::video::Packet packet;

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);

    for (int i = 0; i < 30; ++i)
    {
        changeRect(frame.get(), kChange);

        packet.Clear();
        ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
        EXPECT_LT(dirtyArea(packet), kNoiseSize.width() * kNoiseSize.height());
    }

    EXPECT_TRUE(encoder.isTopOffRequired());

    *frame->updatedRegion() = Region();
    packet.Clear();
    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    EXPECT_EQ(dirtyArea(packet), kNoiseSize.width() * kNoiseSize.height());
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, TopOffRestartsWhenTheFloorDrops)
{
    VideoEncoderVpx encoder;
    encoder.setBandwidth(8 * 1024);

    std::unique_ptr<Frame> frame = makeNoiseFrame(kFrameSize);
    proto::video::Packet packet;

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    ASSERT_LT(encodeTopOff(encoder, frame.get()), 240);

    encoder.setBandwidth(9 * 1024);
    EXPECT_FALSE(encoder.isTopOffRequired());

    encoder.setBandwidth(10 * 1024 * 1024);
    EXPECT_TRUE(encoder.isTopOffRequired());
    EXPECT_LT(encodeTopOff(encoder, frame.get()), 240);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, TopOffIsNotRequiredAtAFixedQuality)
{
    VideoEncoderVpx encoder;
    encoder.setQuality(5);
    encoder.setBandwidth(8 * 1024);

    std::unique_ptr<Frame> frame = makeNoiseFrame(kFrameSize);
    proto::video::Packet packet;

    ASSERT_EQ(encoder.encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    EXPECT_FALSE(encoder.isTopOffRequired());

    encoder.setBandwidth(10 * 1024 * 1024);
    EXPECT_FALSE(encoder.isTopOffRequired());
}

//--------------------------------------------------------------------------------------------------
TEST(WebmVideoEncoderTest, FramesAreDecodable)
{
    VideoEncoderVpx source_encoder;
    std::unique_ptr<VideoDecoder> source_decoder = VideoDecoder::create(proto::video::ENCODING_VP9);
    std::unique_ptr<VideoDecoder> decoder = VideoDecoder::create(proto::video::ENCODING_VP9);
    WebmVideoEncoder encoder;
    bool is_first_frame = true;

    for (int pixels_value : { 0x40, 0x80, 0xC0 })
    {
        std::unique_ptr<Frame> frame = makeFrame(pixels_value);

        proto::video::Packet source_packet;
        ASSERT_EQ(source_encoder.encode(frame.get(), &source_packet), VideoEncoder::Result::SUCCESS);
        ASSERT_EQ(source_decoder->decode(source_packet), VideoDecoder::Result::SUCCESS);

        proto::video::Packet packet;
        ASSERT_TRUE(encoder.encode(source_decoder->frame(), &packet));
        EXPECT_EQ(packet.encoding(), proto::video::ENCODING_VP9);
        EXPECT_EQ(packet.has_format(), is_first_frame);
        EXPECT_FALSE(packet.data().empty());
        ASSERT_EQ(decoder->decode(packet), VideoDecoder::Result::SUCCESS);

        is_first_frame = false;
    }
}
