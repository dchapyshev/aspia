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

#include "base/codec/video_decoder_h264_mf.h"
#include "base/codec/video_encoder_h264_mf.h"

#include <QElapsedTimer>
#include <QThread>

#include <gtest/gtest.h>

#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "base/codec/mf_runtime.h"
#include "base/codec/video_decoder.h"
#include "base/desktop/frame_aligned.h"
#include "base/win/scoped_com_initializer.h"
#include "proto/desktop_video.h"

using Microsoft::WRL::ComPtr;

namespace {

const int kFrameCount = 12;
const QSize kScreenSize(1366, 768);

struct Gray
{
    quint8 left;
    quint8 right;
};

struct Yuv
{
    int y;
    int u;
    int v;
};

// Neighbouring frames differ by 10 in both halves, so a frame returned in place of another one is
// noticed.
Gray grayAt(int index)
{
    const int step = index % 16;
    return { static_cast<quint8>(40 + step * 10), static_cast<quint8>(200 - step * 10) };
}

quint32 argb(quint8 red, quint8 green, quint8 blue)
{
    return 0xFF000000 | (red << 16) | (green << 8) | blue;
}

// The left half is |left|, the right half is |right|.
std::unique_ptr<Frame> makeFrame(const QSize& size, quint32 left, quint32 right)
{
    std::unique_ptr<FrameAligned> frame = FrameAligned::create(size, 32);
    for (int y = 0; y < size.height(); ++y)
    {
        quint32* row = reinterpret_cast<quint32*>(frame->frameData() + y * frame->stride());
        for (int x = 0; x < size.width(); ++x)
            row[x] = (x < size.width() / 2) ? left : right;
    }

    *frame->updatedRegion() += QRect(QPoint(0, 0), size);
    return frame;
}

std::unique_ptr<Frame> makeFrame(const QSize& size, const Gray& gray)
{
    return makeFrame(size, argb(gray.left, gray.left, gray.left),
                     argb(gray.right, gray.right, gray.right));
}

// Random pixels, so the packet size follows the quality the encoder aims for.
std::unique_ptr<Frame> makeNoiseFrame(const QSize& size, std::mt19937& rng)
{
    std::unique_ptr<FrameAligned> frame = FrameAligned::create(size, 32);
    for (int y = 0; y < size.height(); ++y)
    {
        quint32* row = reinterpret_cast<quint32*>(frame->frameData() + y * frame->stride());
        for (int x = 0; x < size.width(); ++x)
            row[x] = 0xFF000000 | (rng() & 0xFFFFFF);
    }

    *frame->updatedRegion() += QRect(QPoint(0, 0), size);
    return frame;
}

// Limited range BT.601, as libyuv converts.
Yuv expectedYuv(quint32 pixel)
{
    const int r = (pixel >> 16) & 0xFF;
    const int g = (pixel >> 8) & 0xFF;
    const int b = pixel & 0xFF;

    return { 16 + (66 * r + 129 * g + 25 * b + 128) / 256,
             128 + (-38 * r - 74 * g + 112 * b + 128) / 256,
             128 + (112 * r - 94 * g - 18 * b + 128) / 256 };
}

int grayLuma(quint8 gray)
{
    return expectedYuv(argb(gray, gray, gray)).y;
}

int luma(const VideoDecoder::YuvView& view, int x, int y)
{
    return view.planeData(0)[y * view.planeStride(0) + x];
}

Yuv yuvAt(const VideoDecoder::YuvView& view, int x, int y)
{
    Yuv pixel = { luma(view, x, y), 0, 0 };

    if (view.format() == VideoDecoder::YuvFormat::NV12)
    {
        const quint8* uv = view.planeData(1) + (y / 2) * view.planeStride(1) + (x / 2) * 2;
        pixel.u = uv[0];
        pixel.v = uv[1];
    }
    else
    {
        pixel.u = view.planeData(1)[(y / 2) * view.planeStride(1) + x / 2];
        pixel.v = view.planeData(2)[(y / 2) * view.planeStride(2) + x / 2];
    }

    return pixel;
}

// Frames fed to a decoder and not yet returned by it. A decoder may keep a packet and return the
// frame with a later one, so the decoded frames are matched against the fed ones in order.
class Checker
{
public:
    void expect(const QSize& size, const Gray& gray) { pending_.push_back({ size, gray }); }
    void reset() { pending_.clear(); }
    int decoded() const { return decoded_; }

    // Checks the frame |decoder| returned for the packet it has just been given.
    void check(VideoDecoder* decoder, VideoDecoder::Result result)
    {
        if (result == VideoDecoder::Result::TEMPORARY_ERROR)
            return;

        ASSERT_EQ(result, VideoDecoder::Result::SUCCESS);
        ASSERT_FALSE(pending_.empty());

        const Expected expected = pending_.front();
        pending_.pop_front();
        ++decoded_;

        const VideoDecoder::YuvView& view = decoder->frame();
        ASSERT_TRUE(view.isValid());
        ASSERT_EQ(view.size(), expected.size);

        const int y = expected.size.height() / 2;
        EXPECT_NEAR(luma(view, expected.size.width() / 4, y), grayLuma(expected.gray.left), 8);
        EXPECT_NEAR(luma(view, expected.size.width() * 3 / 4, y), grayLuma(expected.gray.right), 8);
    }

private:
    struct Expected
    {
        QSize size;
        Gray gray;
    };

    std::deque<Expected> pending_;
    int decoded_ = 0;
};

// Encodes frame |index| of |size| and checks the packet.
proto::video::Packet encodeFrame(VideoEncoderH264MF* encoder, const QSize& size, int index)
{
    const std::unique_ptr<Frame> frame = makeFrame(size, grayAt(index));

    proto::video::Packet packet;
    EXPECT_EQ(encoder->encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS) << index;
    EXPECT_FALSE(packet.data().empty()) << index;
    return packet;
}

void expectFormat(const proto::video::Packet& packet, const QSize& size)
{
    ASSERT_TRUE(packet.has_format());
    EXPECT_EQ(packet.format().video_rect().width(), size.width());
    EXPECT_EQ(packet.format().video_rect().height(), size.height());
    EXPECT_TRUE(packet.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME);
}

// Encodes |frame_count| frames of |size| on |encoder_adapter| and feeds them to |decoder|, waiting
// |delays_ms[i]| before frame i.
void encodeDecode(const QSize& size, IDXGIAdapter* encoder_adapter, VideoDecoder* decoder,
                  int frame_count = kFrameCount, const std::vector<int>& delays_ms = {})
{
    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create(encoder_adapter);
    ASSERT_NE(encoder, nullptr);

    Checker checker;

    for (int i = 0; i < frame_count; ++i)
    {
        if (i < static_cast<int>(delays_ms.size()) && delays_ms[i] > 0)
            QThread::msleep(delays_ms[i]);

        const proto::video::Packet packet = encodeFrame(encoder.get(), size, i);
        if (i == 0)
            expectFormat(packet, size);

        checker.expect(size, grayAt(i));
        checker.check(decoder, decoder->decode(packet));
        if (::testing::Test::HasFatalFailure())
            return;
    }

    EXPECT_GE(checker.decoded(), frame_count - 1);
    GTEST_LOG_(INFO) << checker.decoded() << " of " << frame_count << " frames decoded";
}

// The decoder misses the packets between the first ones and the key frame the client requested
// after that, as when packets are lost.
void keyFrameOnDemand(VideoDecoder* decoder)
{
    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    Checker checker;

    for (int i = 0; i < 5; ++i)
    {
        checker.expect(kScreenSize, grayAt(i));
        checker.check(decoder, decoder->decode(encodeFrame(encoder.get(), kScreenSize, i)));
    }

    for (int i = 5; i < 7; ++i)
        encodeFrame(encoder.get(), kScreenSize, i);

    encoder->setKeyFrameRequired(true);

    for (int i = 7; i < 12; ++i)
    {
        const proto::video::Packet packet = encodeFrame(encoder.get(), kScreenSize, i);
        if (i == 7)
            EXPECT_TRUE(packet.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME);

        checker.expect(kScreenSize, grayAt(i));
        checker.check(decoder, decoder->decode(packet));
    }

    EXPECT_GE(checker.decoded(), 9);
}

// The screen changes its size twice, the second time back to the first size.
void sizeChange(VideoDecoder* decoder)
{
    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    const QSize sizes[] = { kScreenSize, QSize(800, 600), kScreenSize };
    const int frames_per_size = 4;

    Checker checker;
    int index = 0;

    for (const QSize& size : sizes)
    {
        for (int i = 0; i < frames_per_size; ++i, ++index)
        {
            const proto::video::Packet packet = encodeFrame(encoder.get(), size, index);
            if (i == 0)
                expectFormat(packet, size);
            else
                EXPECT_FALSE(packet.has_format()) << index;

            checker.expect(size, grayAt(index));
            checker.check(decoder, decoder->decode(packet));
        }
    }

    EXPECT_GE(checker.decoded(), index - 1);
}

// Red and blue halves, checked in all three planes.
void chroma(VideoDecoder* decoder)
{
    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    const quint32 red = argb(255, 0, 0);
    const quint32 blue = argb(0, 0, 255);
    const std::unique_ptr<Frame> frame = makeFrame(kScreenSize, red, blue);

    // The decoder may return the frame only with a later packet.
    VideoDecoder::Result result = VideoDecoder::Result::TEMPORARY_ERROR;
    for (int i = 0; i < 3 && result != VideoDecoder::Result::SUCCESS; ++i)
    {
        proto::video::Packet packet;
        ASSERT_EQ(encoder->encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS) << i;
        result = decoder->decode(packet);
    }
    ASSERT_EQ(result, VideoDecoder::Result::SUCCESS);

    const VideoDecoder::YuvView& view = decoder->frame();
    ASSERT_TRUE(view.isValid());
    ASSERT_EQ(view.size(), kScreenSize);

    const int y = kScreenSize.height() / 2;
    const Yuv left = yuvAt(view, kScreenSize.width() / 4, y);
    const Yuv right = yuvAt(view, kScreenSize.width() * 3 / 4, y);
    const Yuv expected_left = expectedYuv(red);
    const Yuv expected_right = expectedYuv(blue);

    EXPECT_NEAR(left.y, expected_left.y, 10);
    EXPECT_NEAR(left.u, expected_left.u, 10);
    EXPECT_NEAR(left.v, expected_left.v, 10);
    EXPECT_NEAR(right.y, expected_right.y, 10);
    EXPECT_NEAR(right.u, expected_right.u, 10);
    EXPECT_NEAR(right.v, expected_right.v, 10);
}

// A decoder that joins a stream sees packets without the format first. A new session then starts
// with a new encoder.
void midStreamJoin(VideoDecoder* decoder)
{
    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    for (int i = 0; i < 3; ++i)
        encodeFrame(encoder.get(), kScreenSize, i);

    for (int i = 3; i < 6; ++i)
    {
        const proto::video::Packet packet = encodeFrame(encoder.get(), kScreenSize, i);
        EXPECT_FALSE(packet.has_format()) << i;
        EXPECT_EQ(decoder->decode(packet), VideoDecoder::Result::TEMPORARY_ERROR) << i;
    }

    encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    Checker checker;

    for (int i = 0; i < 4; ++i)
    {
        const proto::video::Packet packet = encodeFrame(encoder.get(), kScreenSize, i);
        if (i == 0)
            expectFormat(packet, kScreenSize);

        checker.expect(kScreenSize, grayAt(i));
        checker.check(decoder, decoder->decode(packet));
    }

    EXPECT_GE(checker.decoded(), 3);
}

// After lost packets the decoder gets frames whose references it has never seen, until the key
// frame the client requests.
void missingReferences(VideoDecoder* decoder)
{
    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    Checker checker;
    checker.expect(kScreenSize, grayAt(0));
    checker.check(decoder, decoder->decode(encodeFrame(encoder.get(), kScreenSize, 0)));

    for (int i = 1; i < 5; ++i)
        encodeFrame(encoder.get(), kScreenSize, i);

    for (int i = 5; i < 8; ++i)
    {
        const VideoDecoder::Result result =
            decoder->decode(encodeFrame(encoder.get(), kScreenSize, i));
        EXPECT_NE(result, VideoDecoder::Result::PERMANENT_ERROR) << i;
    }

    encoder->setKeyFrameRequired(true);
    checker.reset();

    for (int i = 8; i < 13; ++i)
    {
        checker.expect(kScreenSize, grayAt(i));
        checker.check(decoder, decoder->decode(encodeFrame(encoder.get(), kScreenSize, i)));
    }

    EXPECT_GE(checker.decoded(), 5);
}

// A truncated packet, a packet of random bytes and an empty one, then the key frame the client
// requests.
void corruptPacket(VideoDecoder* decoder)
{
    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    Checker checker;

    for (int i = 0; i < 4; ++i)
    {
        checker.expect(kScreenSize, grayAt(i));
        checker.check(decoder, decoder->decode(encodeFrame(encoder.get(), kScreenSize, i)));
    }

    proto::video::Packet packet = encodeFrame(encoder.get(), kScreenSize, 4);
    packet.mutable_data()->resize(packet.data().size() / 2);
    EXPECT_NE(decoder->decode(packet), VideoDecoder::Result::PERMANENT_ERROR) << "truncated";

    std::mt19937 rng(2);
    packet = encodeFrame(encoder.get(), kScreenSize, 5);
    for (char& byte : *packet.mutable_data())
        byte = static_cast<char>(rng());
    EXPECT_NE(decoder->decode(packet), VideoDecoder::Result::PERMANENT_ERROR) << "random";

    packet = encodeFrame(encoder.get(), kScreenSize, 6);
    packet.clear_data();
    EXPECT_EQ(decoder->decode(packet), VideoDecoder::Result::TEMPORARY_ERROR) << "empty";

    encoder->setKeyFrameRequired(true);
    checker.reset();

    for (int i = 7; i < 12; ++i)
    {
        checker.expect(kScreenSize, grayAt(i));
        checker.check(decoder, decoder->decode(encodeFrame(encoder.get(), kScreenSize, i)));
    }

    EXPECT_GE(checker.decoded(), 8);
}

struct SizeOutcome
{
    VideoEncoder::Result result = VideoEncoder::Result::PERMANENT_ERROR;
    bool decoded = false;
    QSize decoded_size;
};

// Encodes one frame of |size| on a fresh encoder on |adapter| and decodes it with the software
// decoder.
SizeOutcome encodeSize(const QSize& size, IDXGIAdapter* adapter)
{
    SizeOutcome outcome;

    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create(adapter);
    if (!encoder)
        return outcome;

    const std::unique_ptr<Frame> frame = makeFrame(size, grayAt(0));
    proto::video::Packet packet;
    outcome.result = encoder->encode(frame.get(), &packet);
    if (outcome.result != VideoEncoder::Result::SUCCESS)
        return outcome;

    std::unique_ptr<VideoDecoder> decoder = VideoDecoder::create(proto::video::ENCODING_H264, false);
    outcome.decoded = decoder && decoder->decode(packet) == VideoDecoder::Result::SUCCESS;
    if (outcome.decoded)
        outcome.decoded_size = decoder->frame().size();

    return outcome;
}

// Runs |task| on its own thread and gives up on it after |timeout| instead of hanging the whole
// test run.
bool runWithWatchdog(const std::function<void()>& task, std::chrono::seconds timeout)
{
    std::shared_ptr<std::promise<void>> done = std::make_shared<std::promise<void>>();
    std::future<void> finished = done->get_future();

    std::thread([task, done]()
    {
        ScopedCOMInitializer com(ScopedCOMInitializer::kMTA);
        task();
        done->set_value();
    }).detach();

    return finished.wait_for(timeout) == std::future_status::ready;
}

std::unique_ptr<VideoDecoder> softwareDecoder()
{
    std::unique_ptr<VideoDecoder> decoder = VideoDecoder::create(proto::video::ENCODING_H264, false);
    EXPECT_NE(decoder, nullptr);
    EXPECT_FALSE(decoder->isHardwareAccelerated());
    return decoder;
}

// Null when the system has no hardware decoder.
std::unique_ptr<VideoDecoder> hardwareDecoder()
{
    std::unique_ptr<VideoDecoder> decoder = VideoDecoder::create(proto::video::ENCODING_H264, true);
    if (!decoder || !decoder->isHardwareAccelerated())
        return nullptr;
    return decoder;
}

std::vector<ComPtr<IDXGIAdapter1>> hardwareAdapters()
{
    std::vector<ComPtr<IDXGIAdapter1>> adapters;

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(mf::createDxgiFactory(&factory)))
        return adapters;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i)
    {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            adapters.push_back(adapter);

        adapter.Reset();
    }

    return adapters;
}

std::string adapterName(IDXGIAdapter* adapter)
{
    DXGI_ADAPTER_DESC desc = {};
    adapter->GetDesc(&desc);
    return QString::fromWCharArray(desc.Description).toStdString();
}

} // namespace

// The frame width is not a multiple of 16, so the rows of the input texture are padded.
TEST(VideoCodecH264MFTest, SoftwareDecode)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    encodeDecode(kScreenSize, nullptr, softwareDecoder().get());
}

TEST(VideoCodecH264MFTest, HardwareDecode)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    std::unique_ptr<VideoDecoder> decoder = hardwareDecoder();
    if (!decoder)
        GTEST_SKIP() << "No hardware H264 decoder";

    encodeDecode(kScreenSize, nullptr, decoder.get());
}

TEST(VideoCodecH264MFTest, Chroma)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    {
        SCOPED_TRACE("Software decoder");
        chroma(softwareDecoder().get());
    }

    if (std::unique_ptr<VideoDecoder> decoder = hardwareDecoder())
    {
        SCOPED_TRACE("Hardware decoder");
        chroma(decoder.get());
    }
}

TEST(VideoCodecH264MFTest, KeyFrameOnDemand)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    {
        SCOPED_TRACE("Software decoder");
        keyFrameOnDemand(softwareDecoder().get());
    }

    if (std::unique_ptr<VideoDecoder> decoder = hardwareDecoder())
    {
        SCOPED_TRACE("Hardware decoder");
        keyFrameOnDemand(decoder.get());
    }
}

TEST(VideoCodecH264MFTest, SizeChange)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    {
        SCOPED_TRACE("Software decoder");
        sizeChange(softwareDecoder().get());
    }

    if (std::unique_ptr<VideoDecoder> decoder = hardwareDecoder())
    {
        SCOPED_TRACE("Hardware decoder");
        sizeChange(decoder.get());
    }
}

TEST(VideoCodecH264MFTest, MidStreamJoin)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    {
        SCOPED_TRACE("Software decoder");
        midStreamJoin(softwareDecoder().get());
    }

    if (std::unique_ptr<VideoDecoder> decoder = hardwareDecoder())
    {
        SCOPED_TRACE("Hardware decoder");
        midStreamJoin(decoder.get());
    }
}

TEST(VideoCodecH264MFTest, MissingReferences)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    {
        SCOPED_TRACE("Software decoder");
        missingReferences(softwareDecoder().get());
    }

    if (std::unique_ptr<VideoDecoder> decoder = hardwareDecoder())
    {
        SCOPED_TRACE("Hardware decoder");
        missingReferences(decoder.get());
    }
}

TEST(VideoCodecH264MFTest, CorruptPacket)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    {
        SCOPED_TRACE("Software decoder");
        corruptPacket(softwareDecoder().get());
    }

    if (std::unique_ptr<VideoDecoder> decoder = hardwareDecoder())
    {
        SCOPED_TRACE("Hardware decoder");
        corruptPacket(decoder.get());
    }
}

// A tiny virtual machine window, 4K, 5K and 8K screens, and a frame above the largest one H.264
// allows at any level (139264 macroblocks), on every adapter with an encoder. A hardware encoder
// may refuse a size, and then the host falls back to a software codec, but it must answer.
TEST(VideoCodecH264MFTest, ExtremeSizes)
{
    const QSize sizes[] = { QSize(64, 64), QSize(320, 200), QSize(3840, 2160), QSize(5120, 2880),
                            QSize(7680, 4320), QSize(8192, 4608) };
    int encoders = 0;

    for (const ComPtr<IDXGIAdapter1>& adapter : hardwareAdapters())
    {
        if (!VideoEncoderH264MF::isHardwareSupported(adapter.Get()))
            continue;

        ++encoders;
        const std::string adapter_name = adapterName(adapter.Get());

        for (const QSize& size : sizes)
        {
            const std::string name = adapter_name + ", " + std::to_string(size.width()) + "x" +
                std::to_string(size.height());
            SCOPED_TRACE(name);

            std::shared_ptr<SizeOutcome> outcome = std::make_shared<SizeOutcome>();
            ASSERT_TRUE(runWithWatchdog(
                [size, adapter, outcome]() { *outcome = encodeSize(size, adapter.Get()); },
                std::chrono::seconds(30))) << "Hang";

            if (outcome->result == VideoEncoder::Result::SUCCESS)
            {
                EXPECT_TRUE(outcome->decoded);
                EXPECT_EQ(outcome->decoded_size, size);
                GTEST_LOG_(INFO) << name << ": encoded";
            }
            else
            {
                EXPECT_EQ(outcome->result, VideoEncoder::Result::PERMANENT_ERROR);
                GTEST_LOG_(INFO) << name << ": refused";
            }
        }
    }

    if (encoders == 0)
        GTEST_SKIP() << "No hardware H264 encoder";
}

// The quality follows the bandwidth: the packets shrink on a narrow link and grow back on a wide
// one. A change of the bandwidth tier recreates the encoder, so it starts with a key frame that
// carries the format.
TEST(VideoCodecH264MFTest, BandwidthChange)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    std::unique_ptr<VideoDecoder> decoder = softwareDecoder();
    std::mt19937 rng(3);

    // Encodes a noise frame and returns the size of its packet.
    auto noisePacket = [&](proto::video::Packet* packet)
    {
        const std::unique_ptr<Frame> frame = makeNoiseFrame(kScreenSize, rng);
        EXPECT_EQ(encoder->encode(frame.get(), packet), VideoEncoder::Result::SUCCESS);
        EXPECT_EQ(decoder->decode(*packet), VideoDecoder::Result::SUCCESS);
        return packet->data().size();
    };

    // The first packet after a tier change, then the average of the following ones.
    auto tierPackets = [&](int count)
    {
        proto::video::Packet first;
        noisePacket(&first);
        expectFormat(first, kScreenSize);

        size_t total = 0;
        for (int i = 0; i < count; ++i)
        {
            proto::video::Packet packet;
            total += noisePacket(&packet);
            EXPECT_FALSE(packet.has_format()) << i;
        }
        return total / count;
    };

    encoder->setBandwidth(4 * 1024 * 1024);
    const size_t high = tierPackets(5);

    encoder->setBandwidth(50 * 1024);
    const size_t low = tierPackets(5);
    EXPECT_LT(low, high * 7 / 10);

    encoder->setBandwidth(4 * 1024 * 1024);
    const size_t high_again = tierPackets(5);
    EXPECT_GT(high_again, low * 13 / 10);

    GTEST_LOG_(INFO) << "Packet sizes: " << high << " -> " << low << " -> " << high_again;
}

// Frames arrive back to back, as when a video plays on the host.
TEST(VideoCodecH264MFTest, ContinuousStream)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    std::unique_ptr<VideoDecoder> decoder = VideoDecoder::create(proto::video::ENCODING_H264, true);
    ASSERT_NE(decoder, nullptr);

    const int frame_count = 300;

    QElapsedTimer timer;
    timer.start();
    encodeDecode(kScreenSize, nullptr, decoder.get(), frame_count);

    GTEST_LOG_(INFO) << frame_count << " frames in " << timer.elapsed() << " ms";
}

// Bursts of frames with short pauses between them and two idle periods of two seconds, as when
// the host sends only what has changed on the screen.
TEST(VideoCodecH264MFTest, RandomCadence)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    std::unique_ptr<VideoDecoder> decoder = VideoDecoder::create(proto::video::ENCODING_H264, true);
    ASSERT_NE(decoder, nullptr);

    const int frame_count = 40;
    std::vector<int> delays_ms(frame_count, 0);

    std::mt19937 rng(1);
    std::uniform_int_distribution<int> pause(1, 150);
    for (int i = 1; i < frame_count; ++i)
    {
        if (rng() % 3 == 0)
            delays_ms[i] = pause(rng);
    }
    delays_ms[frame_count / 3] = 2000;
    delays_ms[frame_count * 2 / 3] = 2000;

    encodeDecode(kScreenSize, nullptr, decoder.get(), frame_count, delays_ms);
}

// Every adapter with an encoder against the software decoder and the hardware decoder of every
// adapter.
TEST(VideoCodecH264MFTest, AdapterMatrix)
{
    const std::vector<ComPtr<IDXGIAdapter1>> adapters = hardwareAdapters();
    int combinations = 0;

    for (const ComPtr<IDXGIAdapter1>& encoder_adapter : adapters)
    {
        const std::string encoder_name = adapterName(encoder_adapter.Get());
        if (!VideoEncoderH264MF::isHardwareSupported(encoder_adapter.Get()))
        {
            GTEST_LOG_(INFO) << "No hardware H264 encoder on " << encoder_name;
            continue;
        }

        {
            GTEST_LOG_(INFO) << "Encoder on " << encoder_name << ", software decoder";
            SCOPED_TRACE("Encoder on " + encoder_name + ", software decoder");

            encodeDecode(kScreenSize, encoder_adapter.Get(), softwareDecoder().get());
            ++combinations;
        }

        for (const ComPtr<IDXGIAdapter1>& decoder_adapter : adapters)
        {
            const std::string decoder_name = adapterName(decoder_adapter.Get());
            GTEST_LOG_(INFO) << "Encoder on " << encoder_name << ", decoder on " << decoder_name;
            SCOPED_TRACE("Encoder on " + encoder_name + ", decoder on " + decoder_name);

            std::unique_ptr<VideoDecoderH264MF> decoder =
                VideoDecoderH264MF::create(decoder_adapter.Get());
            ASSERT_NE(decoder, nullptr);
            encodeDecode(kScreenSize, encoder_adapter.Get(), decoder.get());
            ++combinations;
        }
    }

    if (combinations == 0)
        GTEST_SKIP() << "No hardware H264 encoder";
}

// An odd screen size is encoded without its last column and row.
TEST(VideoCodecH264MFTest, OddSize)
{
    if (!VideoEncoderH264MF::isHardwareSupported())
        GTEST_SKIP() << "No hardware H264 encoder";

    std::unique_ptr<VideoEncoderH264MF> encoder = VideoEncoderH264MF::create();
    ASSERT_NE(encoder, nullptr);

    std::unique_ptr<VideoDecoder> decoder = softwareDecoder();

    const std::unique_ptr<Frame> frame = makeFrame(QSize(1365, 767), { 60, 180 });

    proto::video::Packet packet;
    ASSERT_EQ(encoder->encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
    ASSERT_TRUE(packet.has_format());
    EXPECT_EQ(packet.format().video_rect().width(), 1364);
    EXPECT_EQ(packet.format().video_rect().height(), 766);

    ASSERT_EQ(decoder->decode(packet), VideoDecoder::Result::SUCCESS);
    EXPECT_EQ(decoder->frame().size(), QSize(1364, 766));
}
