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

#include <QRect>
#include <QSize>
#include <QString>
#include <QThread>

#include <gtest/gtest.h>
#include <libyuv/compare.h>
#include <libyuv/convert.h>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <timeapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "winmm.lib")
#else
#include <sys/resource.h>
#endif // defined(Q_OS_WINDOWS)

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "base/time_types.h"
#include "base/codec/video_decoder_vpx.h"
#include "base/codec/video_encoder.h"
#include "base/desktop/frame_aligned.h"
#include "proto/desktop_video.h"

namespace {

const proto::video::Encoding kEncoding = proto::video::ENCODING_VP9;

const QSize kScreenSize(1920, 1080);
const int kFrameRate = 24;
const int kFramesPerScenario = 240;

// One bandwidth inside each tier of VideoEncoderVpx::setBandwidth().
const qint64 kBandwidths[] = { 256 * 1024, 768 * 1024, 1536 * 1024, 4096 * 1024 };

const int kTitleHeight = 32;
const int kTextMargin = 8;
const int kCellWidth = 9;
const int kCellHeight = 18;
const int kGlyphWidth = 8;
const int kGlyphHeight = 16;
const int kGlyphCount = 64;
const int kTextureSize = 256;
const quint32 kDragWindowSeed = 5;

const quint32 kDesktopColor = 0xFF2D5A88;
const quint32 kTaskbarColor = 0xFF1E1E1E;
const quint32 kTitleColor = 0xFFDDDDDD;
const quint32 kWindowColor = 0xFFFFFFFF;
const quint32 kTextColor = 0xFF202020;

const QRect kTaskbarRect(0, 1040, 1920, 40);
const QRect kEditorRect(60, 40, 1100, 960);
const QRect kDragArea(1200, 40, 700, 540);
const QSize kDragWindowSize(360, 240);
const QRect kVideoRect(1240, 620, 640, 360);

// Generator with a fixed seed, so every run encodes exactly the same frames.
class PseudoRandom
{
public:
    explicit PseudoRandom(quint32 seed)
        : state_(seed)
    {
        // Nothing
    }

    quint32 next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    int range(int min, int max)
    {
        return min + static_cast<int>(next() % static_cast<quint32>(max - min + 1));
    }

private:
    quint32 state_;
};

// Synthetic desktop with a text editor, a window that is dragged around and a video player.
class Screen
{
public:
    Screen();

    Frame* frame() const { return frame_.get(); }

    void typeChar();
    void scrollEditor();
    void switchEditorPage();
    void dragWindow();
    void playVideo();

private:
    quint32* pixels(int x, int y) const;
    void fillRect(const QRect& rect, quint32 color);
    void drawGlyph(const QPoint& pos, int index);
    void drawTextLine(const QRect& rect, PseudoRandom* random);
    void drawWindow(const QRect& rect, quint32 seed);
    void drawVideo();

    std::unique_ptr<Frame> frame_;
    PseudoRandom random_;
    quint8 glyphs_[kGlyphCount][kGlyphHeight];
    std::vector<quint8> texture_;
    int type_line_ = 0;
    int type_column_ = 0;
    int type_word_left_ = 0;
    QRect drag_rect_;
    QPoint drag_step_;
    int video_frame_ = 0;

    Q_DISABLE_COPY_MOVE(Screen)
};

struct Scenario
{
    const char* name;
    void (Screen::*step)();
};

const Scenario kScenarios[] =
{
    { "typing", &Screen::typeChar },
    { "scrolling", &Screen::scrollEditor },
    { "page_switch", &Screen::switchEditorPage },
    { "window_drag", &Screen::dragWindow },
    { "video", &Screen::playVideo }
};

struct CpuUsage
{
    MicroSeconds user_time { 0 };
    MicroSeconds kernel_time { 0 };
    quint64 cycles = 0;

    CpuUsage operator-(const CpuUsage& other) const
    {
        return { user_time - other.user_time, kernel_time - other.kernel_time, cycles - other.cycles };
    }

    CpuUsage& operator+=(const CpuUsage& other)
    {
        user_time += other.user_time;
        kernel_time += other.kernel_time;
        cycles += other.cycles;
        return *this;
    }
};

struct RunResult
{
    const char* scenario = nullptr;
    size_t key_frame_bytes = 0;
    MicroSeconds key_frame_time { 0 };
    CpuUsage key_frame_cpu;
    size_t delta_bytes = 0;
    std::vector<MicroSeconds> delta_times;
    CpuUsage delta_cpu;
    std::vector<double> psnr;
};

struct Totals
{
    double bytes = 0;
    double time_ms = 0;
    double cpu_ms = 0;
    double cycles = 0;
    double psnr = 0;

    Totals& operator+=(const Totals& other)
    {
        bytes += other.bytes;
        time_ms += other.time_ms;
        cpu_ms += other.cpu_ms;
        cycles += other.cycles;
        psnr += other.psnr;
        return *this;
    }
};

struct SessionStartResult
{
    size_t first_key_frame_bytes = 0;
    double first_key_frame_psnr = 0;
    size_t second_key_frame_bytes = 0;
    double second_key_frame_psnr = 0;
    size_t second_after_key_frame_bytes = 0;
    double typed_frame_psnr = 0;
    double last_frame_psnr = 0;
    size_t pause_bytes = 0;
    size_t bytes = 0;
};

//--------------------------------------------------------------------------------------------------
QRect textArea(const QRect& window)
{
    const QRect body = window.adjusted(kTextMargin, kTitleHeight + kTextMargin, -kTextMargin, -kTextMargin);
    return QRect(body.left(), body.top(), body.width(), body.height() / kCellHeight * kCellHeight);
}

//--------------------------------------------------------------------------------------------------
Screen::Screen()
    : frame_(FrameAligned::create(kScreenSize, 32)),
      random_(1),
      drag_rect_(kDragArea.topLeft() + QPoint(20, 20), kDragWindowSize),
      drag_step_(14, 9)
{
    PseudoRandom glyph_random(2);
    for (int i = 0; i < kGlyphCount; ++i)
    {
        for (int row = 0; row < kGlyphHeight; ++row)
            glyphs_[i][row] = (row < 3 || row > 12) ? 0 : static_cast<quint8>(glyph_random.next() & 0x7E);
    }

    PseudoRandom texture_random(3);
    std::vector<quint8> noise(kTextureSize * kTextureSize);
    for (quint8& value : noise)
        value = static_cast<quint8>(texture_random.next() >> 24);

    texture_.resize(noise.size());
    for (int y = 0; y < kTextureSize; ++y)
    {
        for (int x = 0; x < kTextureSize; ++x)
        {
            int sum = 0;
            for (int dy = -2; dy <= 2; ++dy)
            {
                for (int dx = -2; dx <= 2; ++dx)
                {
                    const int index = ((y + dy) & (kTextureSize - 1)) * kTextureSize + ((x + dx) & (kTextureSize - 1));
                    sum += noise[static_cast<size_t>(index)];
                }
            }

            // The blur leaves little contrast, so it is stretched back.
            texture_[static_cast<size_t>(y * kTextureSize + x)] =
                static_cast<quint8>(std::clamp((sum / 25 - 128) * 4 + 128, 0, 255));
        }
    }

    fillRect(QRect(QPoint(0, 0), kScreenSize), kDesktopColor);
    fillRect(kTaskbarRect, kTaskbarColor);
    drawWindow(kEditorRect, 4);
    drawWindow(drag_rect_, kDragWindowSeed);
    drawWindow(kVideoRect.adjusted(0, -kTitleHeight, 0, 0), 6);
    drawVideo();

    type_line_ = textArea(kEditorRect).height() / kCellHeight / 2;
    *frame_->updatedRegion() += QRect(QPoint(0, 0), kScreenSize);
}

//--------------------------------------------------------------------------------------------------
void Screen::typeChar()
{
    const QRect text_area = textArea(kEditorRect);
    const int top = text_area.top() + type_line_ * kCellHeight;

    if (type_column_ == 0)
    {
        const QRect line_rect(text_area.left(), top, text_area.width(), kCellHeight);
        fillRect(line_rect, kWindowColor);
        *frame_->updatedRegion() += line_rect;
    }

    const QPoint pos(text_area.left() + type_column_ * kCellWidth, top);
    if (type_word_left_ == 0)
    {
        type_word_left_ = random_.range(2, 9);
    }
    else
    {
        --type_word_left_;
        drawGlyph(pos, random_.range(0, kGlyphCount - 1));
    }

    *frame_->updatedRegion() += QRect(pos, QSize(kCellWidth, kCellHeight));

    if (++type_column_ == text_area.width() / kCellWidth)
    {
        type_column_ = 0;
        type_line_ = (type_line_ + 1) % (text_area.height() / kCellHeight);
    }
}

//--------------------------------------------------------------------------------------------------
void Screen::scrollEditor()
{
    const QRect text_area = textArea(kEditorRect);
    const size_t row_size = static_cast<size_t>(text_area.width()) * sizeof(quint32);

    for (int y = text_area.top(); y + kCellHeight <= text_area.bottom(); ++y)
        memcpy(pixels(text_area.left(), y), pixels(text_area.left(), y + kCellHeight), row_size);

    drawTextLine(QRect(text_area.left(), text_area.bottom() + 1 - kCellHeight, text_area.width(), kCellHeight),
                 &random_);
    *frame_->updatedRegion() += text_area;
}

//--------------------------------------------------------------------------------------------------
void Screen::switchEditorPage()
{
    const QRect text_area = textArea(kEditorRect);

    for (int top = text_area.top(); top < text_area.bottom(); top += kCellHeight)
        drawTextLine(QRect(text_area.left(), top, text_area.width(), kCellHeight), &random_);

    *frame_->updatedRegion() += text_area;
}

//--------------------------------------------------------------------------------------------------
void Screen::dragWindow()
{
    const QRect old_rect = drag_rect_;
    const QRect next_rect = old_rect.translated(drag_step_);

    if (next_rect.left() < kDragArea.left() || next_rect.right() > kDragArea.right())
        drag_step_.setX(-drag_step_.x());
    if (next_rect.top() < kDragArea.top() || next_rect.bottom() > kDragArea.bottom())
        drag_step_.setY(-drag_step_.y());

    drag_rect_ = old_rect.translated(drag_step_);

    fillRect(old_rect, kDesktopColor);
    drawWindow(drag_rect_, kDragWindowSeed);

    *frame_->updatedRegion() += old_rect;
    *frame_->updatedRegion() += drag_rect_;
}

//--------------------------------------------------------------------------------------------------
void Screen::playVideo()
{
    ++video_frame_;
    drawVideo();
    *frame_->updatedRegion() += kVideoRect;
}

//--------------------------------------------------------------------------------------------------
quint32* Screen::pixels(int x, int y) const
{
    return reinterpret_cast<quint32*>(frame_->frameDataAtPos(x, y));
}

//--------------------------------------------------------------------------------------------------
void Screen::fillRect(const QRect& rect, quint32 color)
{
    for (int y = rect.top(); y <= rect.bottom(); ++y)
        std::fill_n(pixels(rect.left(), y), rect.width(), color);
}

//--------------------------------------------------------------------------------------------------
void Screen::drawGlyph(const QPoint& pos, int index)
{
    fillRect(QRect(pos, QSize(kCellWidth, kCellHeight)), kWindowColor);

    for (int row = 0; row < kGlyphHeight; ++row)
    {
        quint32* line = pixels(pos.x(), pos.y() + row);

        for (int column = 0; column < kGlyphWidth; ++column)
        {
            if (glyphs_[index][row] & (1 << column))
                line[column] = kTextColor;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Screen::drawTextLine(const QRect& rect, PseudoRandom* random)
{
    fillRect(rect, kWindowColor);

    const int length = random->range(0, rect.width() / kCellWidth);
    int word_left = random->range(2, 9);

    for (int column = 0; column < length; ++column)
    {
        if (word_left == 0)
        {
            word_left = random->range(2, 9);
            continue;
        }

        --word_left;
        drawGlyph(QPoint(rect.left() + column * kCellWidth, rect.top()), random->range(0, kGlyphCount - 1));
    }
}

//--------------------------------------------------------------------------------------------------
void Screen::drawWindow(const QRect& rect, quint32 seed)
{
    fillRect(QRect(rect.left(), rect.top(), rect.width(), kTitleHeight), kTitleColor);
    fillRect(rect.adjusted(0, kTitleHeight, 0, 0), kWindowColor);

    const QRect text_area = textArea(rect);
    PseudoRandom random(seed);

    for (int top = text_area.top(); top < text_area.bottom(); top += kCellHeight)
        drawTextLine(QRect(text_area.left(), top, text_area.width(), kCellHeight), &random);
}

//--------------------------------------------------------------------------------------------------
void Screen::drawVideo()
{
    const int offset_x = video_frame_ * 3;
    const int offset_y = video_frame_ * 2;
    const int brightness = video_frame_ % 48 - 24;

    for (int y = 0; y < kVideoRect.height(); ++y)
    {
        quint32* line = pixels(kVideoRect.left(), kVideoRect.top() + y);
        const quint8* texture_line =
            &texture_[static_cast<size_t>(((y + offset_y) % kTextureSize) * kTextureSize)];

        for (int x = 0; x < kVideoRect.width(); ++x)
        {
            const quint32 value = static_cast<quint32>(
                std::clamp(texture_line[(x + offset_x) % kTextureSize] + brightness, 0, 255));
            line[x] = 0xFF000000 | (value << 16) | ((value * 3 / 4 + 32) << 8) | (255 - value);
        }
    }
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
MicroSeconds fileTimeToMicroSeconds(const FILETIME& time)
{
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return MicroSeconds(static_cast<qint64>(value.QuadPart / 10));
}
#else
//--------------------------------------------------------------------------------------------------
MicroSeconds timeValToMicroSeconds(const timeval& time)
{
    return Seconds(time.tv_sec) + MicroSeconds(time.tv_usec);
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
// Counts all threads of the process, because libvpx encodes in threads of its own.
CpuUsage processCpuUsage()
{
    CpuUsage usage;

#if defined(Q_OS_WINDOWS)
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;

    if (GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time))
    {
        usage.user_time = fileTimeToMicroSeconds(user_time);
        usage.kernel_time = fileTimeToMicroSeconds(kernel_time);
    }

    ULONG64 cycles = 0;
    if (QueryProcessCycleTime(GetCurrentProcess(), &cycles))
        usage.cycles = cycles;
#else
    rusage resource_usage;
    if (getrusage(RUSAGE_SELF, &resource_usage) == 0)
    {
        usage.user_time = timeValToMicroSeconds(resource_usage.ru_utime);
        usage.kernel_time = timeValToMicroSeconds(resource_usage.ru_stime);
    }
#endif // defined(Q_OS_WINDOWS)

    return usage;
}

//--------------------------------------------------------------------------------------------------
double framePsnr(const Frame& frame, const VideoDecoder::YuvView& decoded, std::vector<quint8>* buffer)
{
    const int width = frame.size().width();
    const int height = frame.size().height();
    const int uv_width = (width + 1) / 2;
    const int uv_height = (height + 1) / 2;

    buffer->resize(static_cast<size_t>(width * height + 2 * uv_width * uv_height));
    quint8* y_data = buffer->data();
    quint8* u_data = y_data + width * height;
    quint8* v_data = u_data + uv_width * uv_height;

    libyuv::ARGBToI420(frame.frameData(), frame.stride(),
                       y_data, width,
                       u_data, uv_width,
                       v_data, uv_width,
                       width, height);

    return libyuv::I420Psnr(y_data, width,
                            u_data, uv_width,
                            v_data, uv_width,
                            decoded.planeData(0), decoded.planeStride(0),
                            decoded.planeData(1), decoded.planeStride(1),
                            decoded.planeData(2), decoded.planeStride(2),
                            width, height);
}

//--------------------------------------------------------------------------------------------------
void runScenario(qint64 bandwidth, const Scenario& scenario, RunResult* result)
{
    std::unique_ptr<VideoEncoder> encoder = VideoEncoder::create(kEncoding);
    ASSERT_NE(encoder, nullptr);

    std::unique_ptr<VideoDecoderVpx> decoder = VideoDecoderVpx::create(kEncoding);
    ASSERT_NE(decoder, nullptr);

    encoder->setBandwidth(bandwidth);
    encoder->setFrameRate(kFrameRate);

    result->scenario = scenario.name;

    Screen screen;
    std::vector<quint8> reference;

    for (int i = 0; i <= kFramesPerScenario; ++i)
    {
        if (i != 0)
            (screen.*scenario.step)();

        proto::video::Packet packet;

        const CpuUsage cpu_before = processCpuUsage();
        const TimePoint start_time = Clock::now();
        const VideoEncoder::Result encode_result = encoder->encode(screen.frame(), &packet);
        const MicroSeconds encode_time = DurationCast<MicroSeconds>(Clock::now() - start_time);
        const CpuUsage cpu = processCpuUsage() - cpu_before;

        ASSERT_EQ(encode_result, VideoEncoder::Result::SUCCESS);

        if (i == 0)
        {
            ASSERT_TRUE(packet.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME);
            result->key_frame_bytes = packet.data().size();
            result->key_frame_time = encode_time;
            result->key_frame_cpu = cpu;
        }
        else
        {
            result->delta_bytes += packet.data().size();
            result->delta_times.push_back(encode_time);
            result->delta_cpu += cpu;
        }

        ASSERT_EQ(decoder->decode(packet), VideoDecoder::Result::SUCCESS);
        result->psnr.push_back(framePsnr(*screen.frame(), decoder->frame(), &reference));

        encoder->setEncodeBuffer(std::move(*packet.mutable_data()));
        *screen.frame()->updatedRegion() = Region();
    }
}

//--------------------------------------------------------------------------------------------------
double toMilliSeconds(MicroSeconds time)
{
    return static_cast<double>(time.count()) / 1000.0;
}

//--------------------------------------------------------------------------------------------------
double cpuMilliSeconds(const CpuUsage& usage)
{
    return toMilliSeconds(usage.user_time + usage.kernel_time);
}

//--------------------------------------------------------------------------------------------------
Totals totals(const RunResult& result)
{
    Totals totals;
    totals.bytes = static_cast<double>(result.key_frame_bytes + result.delta_bytes);
    totals.time_ms = toMilliSeconds(result.key_frame_time);
    totals.cpu_ms = cpuMilliSeconds(result.key_frame_cpu) + cpuMilliSeconds(result.delta_cpu);
    totals.cycles = static_cast<double>(result.key_frame_cpu.cycles + result.delta_cpu.cycles);

    for (MicroSeconds time : result.delta_times)
        totals.time_ms += toMilliSeconds(time);

    for (double psnr : result.psnr)
        totals.psnr += psnr;
    totals.psnr /= static_cast<double>(result.psnr.size());

    return totals;
}

//--------------------------------------------------------------------------------------------------
void printValue(double value, int precision)
{
    std::cout << std::setw(8) << std::fixed << std::setprecision(precision) << value;
}

//--------------------------------------------------------------------------------------------------
// Cycles are counted only on Windows.
void printCycles(double cycles, int precision)
{
    if (cycles > 0)
        printValue(cycles / 1000000.0, precision);
    else
        std::cout << std::setw(8) << "-";
}

//--------------------------------------------------------------------------------------------------
void printRatio(double base, double value)
{
    if (base > 0)
        std::cout << std::setw(7) << std::fixed << std::setprecision(2) << value / base << "x";
    else
        std::cout << std::setw(8) << "-";
}

//--------------------------------------------------------------------------------------------------
void printRun(const RunResult& result)
{
    std::vector<MicroSeconds> times = result.delta_times;
    std::sort(times.begin(), times.end());

    MicroSeconds delta_time(0);
    for (MicroSeconds time : times)
        delta_time += time;

    const double frames = static_cast<double>(times.size());
    const double cpu_ms = cpuMilliSeconds(result.delta_cpu);
    const double kernel_ms = toMilliSeconds(result.delta_cpu.kernel_time);

    std::cout << std::left << std::setw(13) << result.scenario << std::right;

    printValue(static_cast<double>(result.key_frame_bytes) / 1024.0, 1);
    printValue(toMilliSeconds(result.key_frame_time), 2);
    printCycles(static_cast<double>(result.key_frame_cpu.cycles), 1);
    printValue(static_cast<double>(result.delta_bytes) / frames, 0);
    printValue(static_cast<double>(result.delta_bytes) * 8.0 * kFrameRate / frames / 1000.0, 0);
    printValue(toMilliSeconds(delta_time) / frames, 2);
    printValue(toMilliSeconds(times[std::min(times.size() - 1, times.size() * 95 / 100)]), 2);
    printValue(toMilliSeconds(times.back()), 2);
    printValue(cpu_ms / frames, 2);
    printValue(cpu_ms / toMilliSeconds(delta_time), 2);
    printValue(cpu_ms > 0 ? kernel_ms * 100.0 / cpu_ms : 0, 0);
    printCycles(static_cast<double>(result.delta_cpu.cycles) / frames, 2);
    printValue(totals(result).psnr, 2);
    printValue(*std::min_element(result.psnr.begin(), result.psnr.end()), 2);
    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
void printHeader(const std::vector<const char*>& columns)
{
    std::cout << std::left << std::setw(13) << "scenario" << std::right;
    for (const char* column : columns)
        std::cout << std::setw(8) << column;
    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
void printTotals(const char* name, const Totals& totals)
{
    std::cout << std::left << std::setw(13) << name << std::right;

    printValue(totals.bytes / 1024.0, 0);
    printValue(totals.time_ms, 1);
    printValue(totals.cpu_ms, 1);
    printCycles(totals.cycles, 0);
    printValue(totals.psnr, 2);
    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
// |results| holds one entry per scenario.
void printTotalsTable(const std::vector<Totals>& results)
{
    printHeader({ "KB", "ms", "CPU ms", "Mc", "PSNR" });

    Totals total;

    for (size_t i = 0; i < results.size(); ++i)
    {
        printTotals(kScenarios[i].name, results[i]);
        total += results[i];
    }

    total.psnr /= static_cast<double>(results.size());

    printTotals("total", total);
    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
// Typing for 5 s with a second key frame asked for at |second_key_frame|, then 2 s without changes.
// |start_bandwidth| is what the encoder has for the first frame. With |growing| the estimate of the
// bandwidth starts at 32 KB/s and grows by a fifth every second, the way a session starts.
// Otherwise it stays at |start_bandwidth|.
void runKeyFrames(qint64 start_bandwidth, bool growing, int second_key_frame, SessionStartResult* result)
{
    const qint64 kFirstBandwidth = 32 * 1024;
    const int kTypingFrames = 5 * kFrameRate;
    const int kFrameCount = kTypingFrames + 2 * kFrameRate;

    std::unique_ptr<VideoEncoder> encoder = VideoEncoder::create(kEncoding);
    ASSERT_NE(encoder, nullptr);

    std::unique_ptr<VideoDecoderVpx> decoder = VideoDecoderVpx::create(kEncoding);
    ASSERT_NE(decoder, nullptr);

    encoder->setFrameRate(kFrameRate);
    encoder->setBandwidth(start_bandwidth);

    Screen screen;
    std::vector<quint8> reference;
    qint64 bandwidth = kFirstBandwidth;

    for (int i = 0; i < kFrameCount; ++i)
    {
        if (i != 0 && i < kTypingFrames)
            screen.typeChar();

        if (growing && i == 1 && start_bandwidth != kFirstBandwidth)
        {
            encoder->setBandwidth(bandwidth);
        }
        else if (growing && i != 0 && i % kFrameRate == 0)
        {
            bandwidth = bandwidth * 6 / 5;
            encoder->setBandwidth(bandwidth);
        }

        if (i == second_key_frame)
            encoder->setKeyFrameRequired(true);

        proto::video::Packet packet;
        ASSERT_EQ(encoder->encode(screen.frame(), &packet), VideoEncoder::Result::SUCCESS);
        ASSERT_EQ(decoder->decode(packet), VideoDecoder::Result::SUCCESS);

        const double psnr = framePsnr(*screen.frame(), decoder->frame(), &reference);
        const size_t bytes = packet.data().size();

        if (i == 0)
        {
            result->first_key_frame_bytes = bytes;
            result->first_key_frame_psnr = psnr;
        }
        else if (i == second_key_frame)
        {
            result->second_key_frame_bytes = bytes;
            result->second_key_frame_psnr = psnr;
        }

        if (i >= second_key_frame && i < second_key_frame + kFrameRate)
            result->second_after_key_frame_bytes += bytes;

        if (i == kTypingFrames - 1)
            result->typed_frame_psnr = psnr;
        else if (i >= kTypingFrames)
            result->pause_bytes += bytes;

        result->bytes += bytes;
        result->last_frame_psnr = psnr;

        encoder->setEncodeBuffer(std::move(*packet.mutable_data()));
        *screen.frame()->updatedRegion() = Region();
    }
}

//--------------------------------------------------------------------------------------------------
// Copies a plane into a buffer with a border and fills the border from the edge pixels, the way
// VP9 puts every source frame into its lookahead (vp9_copy_and_extend_frame).
void copyAndExtendPlane(const quint8* src, int width, int height, int border, std::vector<quint8>* dst)
{
    const int dst_stride = width + 2 * border;
    dst->resize(static_cast<size_t>(dst_stride * (height + 2 * border)));

    quint8* row = dst->data() + border * dst_stride;
    for (int y = 0; y < height; ++y)
    {
        const quint8* src_row = src + y * width;
        memset(row, src_row[0], static_cast<size_t>(border));
        memcpy(row + border, src_row, static_cast<size_t>(width));
        memset(row + border + width, src_row[width - 1], static_cast<size_t>(border));
        row += dst_stride;
    }

    const quint8* first = dst->data() + border * dst_stride;
    const quint8* last = first + (height - 1) * dst_stride;
    for (int y = 0; y < border; ++y)
    {
        memcpy(dst->data() + y * dst_stride, first, static_cast<size_t>(dst_stride));
        memcpy(dst->data() + (border + height + y) * dst_stride, last, static_cast<size_t>(dst_stride));
    }
}

//--------------------------------------------------------------------------------------------------
// Encodes |kFramesPerScenario| frames after the key frame and its top-off. With |block| one 16x16
// block in the middle of the screen changes in every frame, otherwise nothing changes.
void runFrameCost(const QSize& size, bool block, double* cycles, double* time_ms, double* bytes)
{
    std::unique_ptr<VideoEncoder> encoder = VideoEncoder::create(kEncoding);
    ASSERT_NE(encoder, nullptr);

    encoder->setBandwidth(768 * 1024);
    encoder->setFrameRate(kFrameRate);

    std::unique_ptr<Frame> frame = FrameAligned::create(size, 32);
    PseudoRandom random(7);
    for (int y = 0; y < size.height(); ++y)
    {
        quint32* row = reinterpret_cast<quint32*>(frame->frameDataAtPos(0, y));
        for (int x = 0; x < size.width(); ++x)
        {
            const quint32 value = 0xA0 + (random.next() >> 27);
            row[x] = 0xFF000000 | (value << 16) | (value << 8) | value;
        }
    }

    *frame->updatedRegion() += QRect(QPoint(0, 0), size);

    proto::video::Packet key_packet;
    ASSERT_EQ(encoder->encode(frame.get(), &key_packet), VideoEncoder::Result::SUCCESS);
    encoder->setEncodeBuffer(std::move(*key_packet.mutable_data()));
    *frame->updatedRegion() = Region();

    while (encoder->isTopOffRequired())
    {
        proto::video::Packet packet;
        ASSERT_EQ(encoder->encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
        encoder->setEncodeBuffer(std::move(*packet.mutable_data()));
    }

    const QRect block_rect(QPoint((size.width() / 2) & ~15, (size.height() / 2) & ~15), QSize(16, 16));
    MicroSeconds total_time(0);
    size_t total_bytes = 0;

    const CpuUsage cpu_before = processCpuUsage();

    for (int i = 0; i < kFramesPerScenario; ++i)
    {
        if (block)
        {
            for (int y = block_rect.top(); y <= block_rect.bottom(); ++y)
            {
                quint32* row = reinterpret_cast<quint32*>(frame->frameDataAtPos(block_rect.left(), y));
                for (int x = 0; x < block_rect.width(); ++x)
                    row[x] ^= 0x00FFFFFF;
            }

            *frame->updatedRegion() += block_rect;
        }

        proto::video::Packet packet;

        const TimePoint start_time = Clock::now();
        ASSERT_EQ(encoder->encode(frame.get(), &packet), VideoEncoder::Result::SUCCESS);
        total_time += DurationCast<MicroSeconds>(Clock::now() - start_time);

        total_bytes += packet.data().size();
        encoder->setEncodeBuffer(std::move(*packet.mutable_data()));
        *frame->updatedRegion() = Region();
    }

    const CpuUsage cpu = processCpuUsage() - cpu_before;

    *cycles = static_cast<double>(cpu.cycles) / kFramesPerScenario;
    *time_ms = toMilliSeconds(total_time) / kFramesPerScenario;
    *bytes = static_cast<double>(total_bytes) / kFramesPerScenario;
}

//--------------------------------------------------------------------------------------------------
void encodeScenario(qint64 bandwidth, const Scenario& scenario, std::vector<proto::video::Packet>* packets)
{
    std::unique_ptr<VideoEncoder> encoder = VideoEncoder::create(kEncoding);
    ASSERT_NE(encoder, nullptr);

    encoder->setBandwidth(bandwidth);
    encoder->setFrameRate(kFrameRate);

    Screen screen;

    for (int i = 0; i <= kFramesPerScenario; ++i)
    {
        if (i != 0)
            (screen.*scenario.step)();

        proto::video::Packet packet;
        ASSERT_EQ(encoder->encode(screen.frame(), &packet), VideoEncoder::Result::SUCCESS);
        packets->push_back(std::move(packet));

        *screen.frame()->updatedRegion() = Region();
    }
}

//--------------------------------------------------------------------------------------------------
// Decodes |packets| with a new decoder several times and keeps the fastest pass.
void decodeScenario(const std::vector<proto::video::Packet>& packets, RunResult* result)
{
    const int kPasses = 3;
    double best_time_ms = 0;

    for (int pass = 0; pass < kPasses; ++pass)
    {
        std::unique_ptr<VideoDecoderVpx> decoder = VideoDecoderVpx::create(kEncoding);
        ASSERT_NE(decoder, nullptr);

        RunResult run;
        run.scenario = result->scenario;

        for (size_t i = 0; i < packets.size(); ++i)
        {
            const CpuUsage cpu_before = processCpuUsage();
            const TimePoint start_time = Clock::now();
            const VideoDecoder::Result decode_result = decoder->decode(packets[i]);
            const MicroSeconds decode_time = DurationCast<MicroSeconds>(Clock::now() - start_time);
            const CpuUsage cpu = processCpuUsage() - cpu_before;

            ASSERT_EQ(decode_result, VideoDecoder::Result::SUCCESS);

            if (i == 0)
            {
                run.key_frame_bytes = packets[i].data().size();
                run.key_frame_time = decode_time;
                run.key_frame_cpu = cpu;
            }
            else
            {
                run.delta_bytes += packets[i].data().size();
                run.delta_times.push_back(decode_time);
                run.delta_cpu += cpu;
            }
        }

        double time_ms = toMilliSeconds(run.key_frame_time);
        for (MicroSeconds time : run.delta_times)
            time_ms += toMilliSeconds(time);

        if (pass == 0 || time_ms < best_time_ms)
        {
            best_time_ms = time_ms;
            *result = std::move(run);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void printDecodeRun(const RunResult& result)
{
    std::vector<MicroSeconds> times = result.delta_times;
    std::sort(times.begin(), times.end());

    MicroSeconds delta_time(0);
    for (MicroSeconds time : times)
        delta_time += time;

    const double frames = static_cast<double>(times.size());
    const double cpu_ms = cpuMilliSeconds(result.delta_cpu);

    std::cout << std::left << std::setw(13) << result.scenario << std::right;

    printValue(static_cast<double>(result.key_frame_bytes) / 1024.0, 1);
    printValue(toMilliSeconds(result.key_frame_time), 2);
    printCycles(static_cast<double>(result.key_frame_cpu.cycles), 1);
    printValue(static_cast<double>(result.delta_bytes) / frames, 0);
    printValue(toMilliSeconds(delta_time) / frames, 3);
    printValue(toMilliSeconds(times[std::min(times.size() - 1, times.size() * 95 / 100)]), 2);
    printValue(toMilliSeconds(times.back()), 2);
    printValue(cpu_ms / frames, 3);
    printValue(cpu_ms / toMilliSeconds(delta_time), 2);
    printCycles(static_cast<double>(result.delta_cpu.cycles) / frames, 2);
    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
void printDecodeTotals(const char* name, const Totals& totals)
{
    std::cout << std::left << std::setw(13) << name << std::right;

    printValue(totals.time_ms, 1);
    printValue(totals.cpu_ms, 1);
    printCycles(totals.cycles, 0);
    std::cout << std::endl;
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
// Samples the instruction pointers of the other threads of the process that spent cycles since the
// previous pass. A thread is suspended only while its context is read, and nothing is allocated then:
// a suspended thread may hold the heap lock.
class Sampler
{
public:
    Sampler() { samples_.reserve(kMaxSamples); }
    ~Sampler() { stop(); }

    void start()
    {
        timeBeginPeriod(1);
        thread_ = std::thread(&Sampler::run, this);
    }

    void stop()
    {
        if (!thread_.joinable())
            return;

        stopping_ = true;
        thread_.join();
        timeEndPeriod(1);
    }

    // Prints the functions with the most samples.
    void print(size_t count) const;

private:
    struct Target
    {
        DWORD id;
        HANDLE handle;
        ULONG64 cycles;
    };

    void run();
    void updateTargets(std::vector<Target>* targets) const;

    static const size_t kMaxSamples = 1 << 22;

    std::vector<DWORD64> samples_;
    std::atomic<bool> stopping_ { false };
    std::thread thread_;
};

//--------------------------------------------------------------------------------------------------
void Sampler::updateTargets(std::vector<Target>* targets) const
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    THREADENTRY32 entry;
    entry.dwSize = sizeof(entry);

    for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry))
    {
        if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == GetCurrentThreadId())
            continue;

        const bool known = std::any_of(targets->begin(), targets->end(),
                                       [&](const Target& target) { return target.id == entry.th32ThreadID; });
        if (known)
            continue;

        HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
                                   entry.th32ThreadID);
        if (handle)
            targets->push_back({ entry.th32ThreadID, handle, 0 });
    }

    CloseHandle(snapshot);
}

//--------------------------------------------------------------------------------------------------
void Sampler::run()
{
    std::vector<Target> targets;

    for (int pass = 0; !stopping_; ++pass)
    {
        if (pass % 20 == 0)
            updateTargets(&targets);

        for (Target& target : targets)
        {
            ULONG64 cycles = 0;
            if (!QueryThreadCycleTime(target.handle, &cycles) || cycles == target.cycles)
                continue;
            target.cycles = cycles;

            if (SuspendThread(target.handle) == static_cast<DWORD>(-1))
                continue;

            CONTEXT context;
            memset(&context, 0, sizeof(context));
            context.ContextFlags = CONTEXT_CONTROL;

            if (GetThreadContext(target.handle, &context) && samples_.size() < kMaxSamples)
            {
                DWORD64 address = 0;
#if defined(Q_PROCESSOR_X86_64)
                address = context.Rip;
#elif defined(Q_PROCESSOR_X86_32)
                address = context.Eip;
#elif defined(Q_PROCESSOR_ARM_64)
                address = context.Pc;
#endif
                samples_.push_back(address);
            }

            ResumeThread(target.handle);
        }

        Sleep(1);
    }

    for (const Target& target : targets)
        CloseHandle(target.handle);
}

//--------------------------------------------------------------------------------------------------
void Sampler::print(size_t count) const
{
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    std::unordered_map<DWORD64, std::string> names;
    std::map<std::string, size_t> counts;
    std::vector<char> buffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);

    for (DWORD64 address : samples_)
    {
        auto name = names.find(address);
        if (name == names.end())
        {
            std::fill(buffer.begin(), buffer.end(), 0);
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            DWORD64 displacement = 0;
            name = names.emplace(address, SymFromAddr(process, address, &displacement, symbol) ? symbol->Name : "?")
                       .first;
        }

        ++counts[name->second];
    }

    SymCleanup(process);

    std::vector<std::pair<std::string, size_t>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    const double total = static_cast<double>(samples_.size());
    double sum = 0;

    std::cout << "samples: " << samples_.size() << std::endl;
    for (size_t i = 0; i < std::min(count, sorted.size()); ++i)
    {
        sum += static_cast<double>(sorted[i].second);
        std::cout << std::setw(7) << std::fixed << std::setprecision(1)
                  << static_cast<double>(sorted[i].second) * 100.0 / total << "%" << std::setw(7)
                  << sum * 100.0 / total << "%  " << sorted[i].first << std::endl;
    }
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, DISABLED_ScreenContentBenchmark)
{
    std::vector<Totals> all_bandwidths(std::size(kScenarios));

    std::cout << std::endl
              << "=== Screen content: " << kScreenSize.width() << "x" << kScreenSize.height() << ", "
              << kFrameRate << " fps, " << kFramesPerScenario << " delta frames per scenario, "
              << QThread::idealThreadCount() << " logical CPUs ===" << std::endl
              << "key: the first frame of a scenario. Other columns are per delta frame. Mc: millions of CPU"
              << std::endl
              << "cycles of the process. cores: CPU time / encoding time. PSNR (dB) is over all frames."
              << std::endl;

    for (qint64 bandwidth : kBandwidths)
    {
        std::vector<RunResult> results;

        for (const Scenario& scenario : kScenarios)
        {
            RunResult result;
            ASSERT_NO_FATAL_FAILURE(runScenario(bandwidth, scenario, &result));
            results.push_back(std::move(result));
        }

        std::cout << std::endl << "=== Bandwidth " << bandwidth / 1024 << " KB/s ===" << std::endl << std::endl;

        printHeader({ "key KB", "key ms", "key Mc", "B/frame", "kbit/s", "avg ms", "p95 ms", "max ms", "CPU ms",
                      "cores", "kern %", "Mc", "PSNR", "PSNRmin" });

        std::vector<Totals> scenario_totals;

        for (size_t i = 0; i < results.size(); ++i)
        {
            printRun(results[i]);

            scenario_totals.push_back(totals(results[i]));
            all_bandwidths[i] += scenario_totals[i];
        }

        std::cout << std::endl;
        printTotalsTable(scenario_totals);
    }

    for (Totals& total : all_bandwidths)
        total.psnr /= static_cast<double>(std::size(kBandwidths));

    std::cout << "=== Over all bandwidths ===" << std::endl << std::endl;
    printTotalsTable(all_bandwidths);
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, DISABLED_SessionStartBenchmark)
{
    const qint64 start_bandwidths[] = { 0, 32 * 1024 };

    std::cout << std::endl
              << "=== Session start: " << kScreenSize.width() << "x" << kScreenSize.height() << ", " << kFrameRate
              << " fps, typing for 5 s and a pause of 2 s, the bandwidth starts at 32 KB/s and grows by a fifth"
              << " every second ===" << std::endl
              << "start: the bandwidth the encoder has for the first key frame (0 is unknown). The second key frame"
              << std::endl
              << "is requested 0.6 s after the first one. type dB: the end of typing, last dB: the end of the pause."
              << std::endl
              << "idle KB: what goes out in the pause. PSNR (dB) of the whole screen." << std::endl << std::endl;

    std::cout << std::left << std::setw(13) << "start KB/s" << std::right;
    for (const char* column : { "key1 KB", "key1 dB", "key2 KB", "key2 dB", "type dB", "last dB", "idle KB",
                                "all KB" })
        std::cout << std::setw(8) << column;
    std::cout << std::endl;

    for (qint64 start_bandwidth : start_bandwidths)
    {
        SessionStartResult result;
        ASSERT_NO_FATAL_FAILURE(runKeyFrames(start_bandwidth, true, 14, &result));

        std::cout << std::left << std::setw(13) << start_bandwidth / 1024 << std::right;

        printValue(static_cast<double>(result.first_key_frame_bytes) / 1024.0, 1);
        printValue(result.first_key_frame_psnr, 2);
        printValue(static_cast<double>(result.second_key_frame_bytes) / 1024.0, 1);
        printValue(result.second_key_frame_psnr, 2);
        printValue(result.typed_frame_psnr, 2);
        printValue(result.last_frame_psnr, 2);
        printValue(static_cast<double>(result.pause_bytes) / 1024.0, 0);
        printValue(static_cast<double>(result.bytes) / 1024.0, 0);
        std::cout << std::endl;
    }

    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, DISABLED_KeyFrameRequestBenchmark)
{
    const int kRequestedKeyFrame = 3 * kFrameRate;

    std::cout << std::endl
              << "=== Requested key frame: " << kScreenSize.width() << "x" << kScreenSize.height() << ", "
              << kFrameRate << " fps, typing for 5 s and a pause of 2 s on a steady bandwidth, a key frame is"
              << " requested at 3 s ==="
              << std::endl
              << "key2 budget: the size of the requested key frame in frame budgets (bandwidth / fps). key2 ms: the"
              << std::endl
              << "time to send it. 1s KB: what goes out in the second from it. type dB: the end of typing, last dB:"
              << std::endl
              << "the end of the pause. PSNR (dB) of the whole screen." << std::endl << std::endl;

    std::cout << std::left << std::setw(13) << "bw KB/s" << std::right;
    for (const char* column :
         { "key1 KB", "key1 dB", "key2 KB", "key2 dB", "budget", "key2 ms", "1s KB", "type dB", "last dB" })
        std::cout << std::setw(8) << column;
    std::cout << std::endl;

    for (qint64 bandwidth : kBandwidths)
    {
        SessionStartResult result;
        ASSERT_NO_FATAL_FAILURE(runKeyFrames(bandwidth, false, kRequestedKeyFrame, &result));

        const double key_frame_bytes = static_cast<double>(result.second_key_frame_bytes);

        std::cout << std::left << std::setw(13) << bandwidth / 1024 << std::right;

        printValue(static_cast<double>(result.first_key_frame_bytes) / 1024.0, 1);
        printValue(result.first_key_frame_psnr, 2);
        printValue(key_frame_bytes / 1024.0, 1);
        printValue(result.second_key_frame_psnr, 2);
        printValue(key_frame_bytes * kFrameRate / static_cast<double>(bandwidth), 1);
        printValue(key_frame_bytes * 1000.0 / static_cast<double>(bandwidth), 0);
        printValue(static_cast<double>(result.second_after_key_frame_bytes) / 1024.0, 1);
        printValue(result.typed_frame_psnr, 2);
        printValue(result.last_frame_psnr, 2);
        std::cout << std::endl;
    }

    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, DISABLED_FrameCostBenchmark)
{
    const QSize sizes[] = { QSize(1280, 720), QSize(1920, 1080), QSize(2560, 1440) };

    std::cout << std::endl
              << "=== Frame cost: " << kFramesPerScenario
              << " frames after the key frame and its top-off at 768 KB/s ===" << std::endl
              << "empty: nothing changes. block: one 16x16 block changes. copy: one copy of an I420 frame with a"
              << std::endl
              << "border of 160 pixels, the way VP9 puts each source frame into its lookahead. Mc: millions of"
              << std::endl
              << "CPU cycles of the process per frame." << std::endl << std::endl;

    std::cout << std::left << std::setw(13) << "size" << std::right;
    for (const char* column : { "mode", "Mc", "ms", "B/frame" })
        std::cout << std::setw(8) << column;
    std::cout << std::endl;

    for (const QSize& size : sizes)
    {
        const QString name = QString("%1x%2").arg(size.width()).arg(size.height());

        for (bool block : { false, true })
        {
            double cycles = 0;
            double time_ms = 0;
            double bytes = 0;
            ASSERT_NO_FATAL_FAILURE(runFrameCost(size, block, &cycles, &time_ms, &bytes));

            std::cout << std::left << std::setw(13) << name.toStdString() << std::right << std::setw(8)
                      << (block ? "block" : "empty");
            printCycles(cycles, 2);
            printValue(time_ms, 3);
            printValue(bytes, 0);
            std::cout << std::endl;
        }

        const int uv_width = size.width() / 2;
        const int uv_height = size.height() / 2;
        std::vector<quint8> source(static_cast<size_t>(size.width() * size.height()), 128);
        std::vector<quint8> y_plane;
        std::vector<quint8> u_plane;
        std::vector<quint8> v_plane;

        const CpuUsage cpu_before = processCpuUsage();
        const TimePoint start_time = Clock::now();

        for (int i = 0; i < kFramesPerScenario; ++i)
        {
            copyAndExtendPlane(source.data(), size.width(), size.height(), 160, &y_plane);
            copyAndExtendPlane(source.data(), uv_width, uv_height, 80, &u_plane);
            copyAndExtendPlane(source.data(), uv_width, uv_height, 80, &v_plane);
        }

        const double time_ms = toMilliSeconds(DurationCast<MicroSeconds>(Clock::now() - start_time));
        const CpuUsage cpu = processCpuUsage() - cpu_before;

        std::cout << std::left << std::setw(13) << name.toStdString() << std::right << std::setw(8) << "copy";
        printCycles(static_cast<double>(cpu.cycles) / kFramesPerScenario, 2);
        printValue(time_ms / kFramesPerScenario, 3);
        std::cout << std::endl;
    }

    std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
TEST(VideoDecoderVpxTest, DISABLED_DecodeBenchmark)
{
    std::cout << std::endl
              << "=== Decoding: " << kScreenSize.width() << "x" << kScreenSize.height() << ", " << kFrameRate
              << " fps, " << kFramesPerScenario << " delta frames per scenario, " << QThread::idealThreadCount()
              << " logical CPUs ===" << std::endl
              << "The streams of the encoder benchmark, each decoded 3 times, the fastest pass is shown. key: the"
              << std::endl
              << "first frame. Other columns are per delta frame. Mc: millions of CPU cycles of the process."
              << std::endl
              << "cores: CPU time / decoding time. The last table sums all frames of a scenario." << std::endl;

    for (qint64 bandwidth : kBandwidths)
    {
        std::vector<RunResult> results;

        for (const Scenario& scenario : kScenarios)
        {
            std::vector<proto::video::Packet> packets;
            ASSERT_NO_FATAL_FAILURE(encodeScenario(bandwidth, scenario, &packets));

            RunResult result;
            result.scenario = scenario.name;
            ASSERT_NO_FATAL_FAILURE(decodeScenario(packets, &result));
            results.push_back(std::move(result));
        }

        std::cout << std::endl << "=== Bandwidth " << bandwidth / 1024 << " KB/s ===" << std::endl << std::endl;

        printHeader({ "key KB", "key ms", "key Mc", "B/frame", "avg ms", "p95 ms", "max ms", "CPU ms", "cores", "Mc" });

        for (const RunResult& result : results)
            printDecodeRun(result);

        std::cout << std::endl;
        printHeader({ "ms", "CPU ms", "Mc" });

        Totals total;

        for (const RunResult& result : results)
        {
            const Totals run = totals(result);

            printDecodeTotals(result.scenario, run);
            total += run;
        }

        printDecodeTotals("total", total);
    }

    std::cout << std::endl;
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
TEST(VideoEncoderVpxTest, DISABLED_ProfileBenchmark)
{
    const qint64 bandwidth = 4096 * 1024;
    const int kRepeats = 3;

    std::cout << std::endl
              << "=== Profile: VP9, " << kScreenSize.width() << "x" << kScreenSize.height() << ", "
              << bandwidth / 1024 << " KB/s, each scenario encoded " << kRepeats << " times ===" << std::endl
              << "Samples of the instruction pointer of all threads of the process that run. Columns: the share"
              << std::endl
              << "of the function and the running sum." << std::endl;

    for (const Scenario& scenario : kScenarios)
    {
        Sampler sampler;
        sampler.start();

        for (int i = 0; i < kRepeats; ++i)
        {
            std::vector<proto::video::Packet> packets;
            ASSERT_NO_FATAL_FAILURE(encodeScenario(bandwidth, scenario, &packets));
        }

        sampler.stop();

        std::cout << std::endl << "=== " << scenario.name << " ===" << std::endl;
        sampler.print(30);
    }

    std::cout << std::endl;
}
#endif // defined(Q_OS_WINDOWS)
