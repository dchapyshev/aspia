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

#include "base/codec/scale_reducer.h"

#include "base/desktop/frame_aligned.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>

#include <iostream>

namespace base {

namespace {

const int kAlignment = 32;

std::unique_ptr<Frame> createTestFrame(const QSize& size)
{
    auto frame = FrameAligned::create(size, kAlignment);
    EXPECT_NE(frame, nullptr);
    if (!frame)
        return nullptr;

    quint8* data = frame->frameData();
    const int stride = frame->stride();
    const int height = size.height();
    const int width = size.width();

    // Fill with a gradient pattern.
    for (int y = 0; y < height; ++y)
    {
        quint32* row = reinterpret_cast<quint32*>(data + y * stride);
        for (int x = 0; x < width; ++x)
        {
            quint8 r = static_cast<quint8>((x * 255) / (width > 1 ? width - 1 : 1));
            quint8 g = static_cast<quint8>((y * 255) / (height > 1 ? height - 1 : 1));
            quint8 b = static_cast<quint8>(((x + y) * 128) / (width + height > 2 ? width + height - 2 : 1));
            row[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }

    *frame->updatedRegion() += QRect(QPoint(0, 0), size);
    return frame;
}

std::unique_ptr<Frame> createSolidFrame(const QSize& size, quint32 color)
{
    auto frame = FrameAligned::create(size, kAlignment);
    EXPECT_NE(frame, nullptr);
    if (!frame)
        return nullptr;

    quint8* data = frame->frameData();
    const int stride = frame->stride();

    for (int y = 0; y < size.height(); ++y)
    {
        quint32* row = reinterpret_cast<quint32*>(data + y * stride);
        for (int x = 0; x < size.width(); ++x)
            row[x] = color;
    }

    *frame->updatedRegion() += QRect(QPoint(0, 0), size);
    return frame;
}

const char* qualityName(ScaleReducer::Quality quality)
{
    switch (quality)
    {
        case ScaleReducer::Quality::LOW:
            return "LOW";
        case ScaleReducer::Quality::NORMAL:
            return "NORMAL";
        case ScaleReducer::Quality::HIGH:
            return "HIGH";
        default:
            return "UNKNOWN";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Unit Tests
//--------------------------------------------------------------------------------------------------

TEST(scale_reducer_test, same_size_returns_source)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize size(1920, 1080);
    auto frame = createTestFrame(size);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), size);
    ASSERT_NE(result, nullptr);
    // When source and target sizes are the same, the source frame is returned directly.
    EXPECT_EQ(result, frame.get());
}

TEST(scale_reducer_test, downscale_half)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);
    EXPECT_NE(result, frame.get());
    EXPECT_EQ(result->size(), target_size);
}

TEST(scale_reducer_test, downscale_quarter)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(480, 270);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->size(), target_size);
}

TEST(scale_reducer_test, non_proportional_scale)
{
    ScaleReducer reducer(ScaleReducer::Quality::NORMAL);
    QSize source_size(1920, 1080);
    QSize target_size(1280, 720);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->size(), target_size);
}

TEST(scale_reducer_test, scale_factors)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    reducer.scaleFrame(frame.get(), target_size);

    // scale = target * 100.0 / source
    double expected_x = (960.0 * 100.0) / 1920.0;  // 50.0
    double expected_y = (540.0 * 100.0) / 1080.0;   // 50.0
    EXPECT_DOUBLE_EQ(reducer.scaleFactorX(), expected_x);
    EXPECT_DOUBLE_EQ(reducer.scaleFactorY(), expected_y);
}

TEST(scale_reducer_test, scale_factors_non_uniform)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(1280, 540);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    reducer.scaleFrame(frame.get(), target_size);

    double expected_x = (1280.0 * 100.0) / 1920.0;
    double expected_y = (540.0 * 100.0) / 1080.0;
    EXPECT_NEAR(reducer.scaleFactorX(), expected_x, 0.001);
    EXPECT_NEAR(reducer.scaleFactorY(), expected_y, 0.001);
}

TEST(scale_reducer_test, updated_region_first_call)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);

    // On first call, entire target frame should be in updated region.
    EXPECT_FALSE(result->constUpdatedRegion().isEmpty());
    EXPECT_TRUE(result->constUpdatedRegion().contains(QRect(QPoint(0, 0), target_size)));
}

TEST(scale_reducer_test, partial_update)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    // First call - full frame.
    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);

    // Second call - partial update.
    *frame->updatedRegion() = QRegion();
    QRect update_rect(100, 100, 200, 200);
    *frame->updatedRegion() += update_rect;

    result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);

    // Updated region should not be empty.
    EXPECT_FALSE(result->constUpdatedRegion().isEmpty());
}

TEST(scale_reducer_test, size_change_resets_frame)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    // First scale to 960x540.
    QSize target1(960, 540);
    const Frame* result1 = reducer.scaleFrame(frame.get(), target1);
    ASSERT_NE(result1, nullptr);
    EXPECT_EQ(result1->size(), target1);

    // Then scale to 640x360 - should create a new target frame.
    QSize target2(640, 360);
    const Frame* result2 = reducer.scaleFrame(frame.get(), target2);
    ASSERT_NE(result2, nullptr);
    EXPECT_EQ(result2->size(), target2);
}

TEST(scale_reducer_test, solid_white_frame)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    auto frame = createSolidFrame(source_size, 0xFFFFFFFF);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);

    // After scaling a solid white frame, all pixels should remain white.
    const quint8* data = result->frameData();
    const int stride = result->stride();
    bool all_white = true;
    for (int y = 0; y < target_size.height() && all_white; ++y)
    {
        const quint32* row = reinterpret_cast<const quint32*>(data + y * stride);
        for (int x = 0; x < target_size.width(); ++x)
        {
            if (row[x] != 0xFFFFFFFF)
            {
                all_white = false;
                break;
            }
        }
    }
    EXPECT_TRUE(all_white) << "Solid white frame should remain white after scaling";
}

TEST(scale_reducer_test, solid_black_frame)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    auto frame = createSolidFrame(source_size, 0xFF000000);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);

    const quint8* data = result->frameData();
    const int stride = result->stride();
    bool all_black = true;
    for (int y = 0; y < target_size.height() && all_black; ++y)
    {
        const quint32* row = reinterpret_cast<const quint32*>(data + y * stride);
        for (int x = 0; x < target_size.width(); ++x)
        {
            if (row[x] != 0xFF000000)
            {
                all_black = false;
                break;
            }
        }
    }
    EXPECT_TRUE(all_black) << "Solid black frame should remain black after scaling";
}

TEST(scale_reducer_test, small_frame)
{
    ScaleReducer reducer(ScaleReducer::Quality::LOW);
    QSize source_size(100, 100);
    QSize target_size(50, 50);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->size(), target_size);
}

TEST(scale_reducer_test, odd_dimensions)
{
    ScaleReducer reducer(ScaleReducer::Quality::HIGH);
    QSize source_size(1921, 1081);
    QSize target_size(961, 541);

    auto frame = createTestFrame(source_size);
    ASSERT_NE(frame, nullptr);

    const Frame* result = reducer.scaleFrame(frame.get(), target_size);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->size(), target_size);
}

TEST(scale_reducer_test, multiple_sequential_frames)
{
    ScaleReducer reducer(ScaleReducer::Quality::NORMAL);
    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    for (int i = 0; i < 10; ++i)
    {
        auto frame = createSolidFrame(source_size, 0xFF000000 | static_cast<quint32>(i * 25));
        ASSERT_NE(frame, nullptr);

        const Frame* result = reducer.scaleFrame(frame.get(), target_size);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->size(), target_size);
    }
}

TEST(scale_reducer_test, all_quality_levels_produce_valid_output)
{
    const ScaleReducer::Quality qualities[] =
    {
        ScaleReducer::Quality::LOW,
        ScaleReducer::Quality::NORMAL,
        ScaleReducer::Quality::HIGH
    };

    QSize source_size(1920, 1080);
    QSize target_size(960, 540);

    for (auto quality : qualities)
    {
        ScaleReducer reducer(quality);
        auto frame = createTestFrame(source_size);
        ASSERT_NE(frame, nullptr) << "Failed for quality: " << qualityName(quality);

        const Frame* result = reducer.scaleFrame(frame.get(), target_size);
        ASSERT_NE(result, nullptr) << "Failed for quality: " << qualityName(quality);
        EXPECT_EQ(result->size(), target_size) << "Failed for quality: " << qualityName(quality);
    }
}

//--------------------------------------------------------------------------------------------------
// Benchmarks
//--------------------------------------------------------------------------------------------------

TEST(scale_reducer_test, benchmark_quality_comparison)
{
    const ScaleReducer::Quality qualities[] =
    {
        ScaleReducer::Quality::LOW,
        ScaleReducer::Quality::NORMAL,
        ScaleReducer::Quality::HIGH
    };

    QSize source_size(1920, 1080);
    QSize target_size(960, 540);
    const int kIterations = 500;

    std::cout << "=== Quality comparison: 1920x1080 -> 960x540 ===" << std::endl;

    for (auto quality : qualities)
    {
        ScaleReducer reducer(quality);
        auto frame = createTestFrame(source_size);
        ASSERT_NE(frame, nullptr);

        // Warm up.
        reducer.scaleFrame(frame.get(), target_size);

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < kIterations; ++i)
        {
            *frame->updatedRegion() = QRegion();
            *frame->updatedRegion() += QRect(QPoint(0, 0), source_size);
            reducer.scaleFrame(frame.get(), target_size);
        }

        qint64 elapsed = timer.elapsed();
        double ms_per_frame = static_cast<double>(elapsed) / kIterations;
        double fps = 1000.0 / ms_per_frame;

        std::cout << "  " << qualityName(quality) << ": "
                  << ms_per_frame << " ms/frame, "
                  << fps << " fps" << std::endl;
    }
}

TEST(scale_reducer_test, benchmark_quality_comparison_4k)
{
    const ScaleReducer::Quality qualities[] =
    {
        ScaleReducer::Quality::LOW,
        ScaleReducer::Quality::NORMAL,
        ScaleReducer::Quality::HIGH
    };

    QSize source_size(3840, 2160);
    QSize target_size(1920, 1080);
    const int kIterations = 200;

    std::cout << "=== Quality comparison: 3840x2160 -> 1920x1080 ===" << std::endl;

    for (auto quality : qualities)
    {
        ScaleReducer reducer(quality);
        auto frame = createTestFrame(source_size);
        ASSERT_NE(frame, nullptr);

        reducer.scaleFrame(frame.get(), target_size);

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < kIterations; ++i)
        {
            *frame->updatedRegion() = QRegion();
            *frame->updatedRegion() += QRect(QPoint(0, 0), source_size);
            reducer.scaleFrame(frame.get(), target_size);
        }

        qint64 elapsed = timer.elapsed();
        double ms_per_frame = static_cast<double>(elapsed) / kIterations;
        double fps = 1000.0 / ms_per_frame;

        std::cout << "  " << qualityName(quality) << ": "
                  << ms_per_frame << " ms/frame, "
                  << fps << " fps" << std::endl;
    }
}

TEST(scale_reducer_test, benchmark_quality_comparison_quarter)
{
    const ScaleReducer::Quality qualities[] =
    {
        ScaleReducer::Quality::LOW,
        ScaleReducer::Quality::NORMAL,
        ScaleReducer::Quality::HIGH
    };

    QSize source_size(1920, 1080);
    QSize target_size(480, 270);
    const int kIterations = 500;

    std::cout << "=== Quality comparison: 1920x1080 -> 480x270 ===" << std::endl;

    for (auto quality : qualities)
    {
        ScaleReducer reducer(quality);
        auto frame = createTestFrame(source_size);
        ASSERT_NE(frame, nullptr);

        reducer.scaleFrame(frame.get(), target_size);

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < kIterations; ++i)
        {
            *frame->updatedRegion() = QRegion();
            *frame->updatedRegion() += QRect(QPoint(0, 0), source_size);
            reducer.scaleFrame(frame.get(), target_size);
        }

        qint64 elapsed = timer.elapsed();
        double ms_per_frame = static_cast<double>(elapsed) / kIterations;
        double fps = 1000.0 / ms_per_frame;

        std::cout << "  " << qualityName(quality) << ": "
                  << ms_per_frame << " ms/frame, "
                  << fps << " fps" << std::endl;
    }
}

TEST(scale_reducer_test, benchmark_partial_update_small)
{
    const ScaleReducer::Quality qualities[] =
    {
        ScaleReducer::Quality::LOW,
        ScaleReducer::Quality::NORMAL,
        ScaleReducer::Quality::HIGH
    };

    QSize source_size(1920, 1080);
    QSize target_size(960, 540);
    const int kIterations = 5000;

    std::cout << "=== Partial update 64x64: 1920x1080 -> 960x540 ===" << std::endl;

    for (auto quality : qualities)
    {
        ScaleReducer reducer(quality);
        auto frame = createTestFrame(source_size);
        ASSERT_NE(frame, nullptr);

        // First call - full frame.
        reducer.scaleFrame(frame.get(), target_size);

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < kIterations; ++i)
        {
            // Small dirty region (cursor area).
            *frame->updatedRegion() = QRegion();
            *frame->updatedRegion() += QRect(100, 100, 64, 64);
            reducer.scaleFrame(frame.get(), target_size);
        }

        qint64 elapsed = timer.elapsed();
        double ms_per_frame = static_cast<double>(elapsed) / kIterations;
        double fps = 1000.0 / ms_per_frame;

        std::cout << "  " << qualityName(quality) << ": "
                  << ms_per_frame << " ms/frame, "
                  << fps << " fps" << std::endl;
    }
}

TEST(scale_reducer_test, benchmark_partial_update_medium)
{
    const ScaleReducer::Quality qualities[] =
    {
        ScaleReducer::Quality::LOW,
        ScaleReducer::Quality::NORMAL,
        ScaleReducer::Quality::HIGH
    };

    QSize source_size(1920, 1080);
    QSize target_size(960, 540);
    const int kIterations = 2000;

    std::cout << "=== Partial update 640x480: 1920x1080 -> 960x540 ===" << std::endl;

    for (auto quality : qualities)
    {
        ScaleReducer reducer(quality);
        auto frame = createTestFrame(source_size);
        ASSERT_NE(frame, nullptr);

        reducer.scaleFrame(frame.get(), target_size);

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < kIterations; ++i)
        {
            // Medium dirty region (window move).
            *frame->updatedRegion() = QRegion();
            *frame->updatedRegion() += QRect(200, 150, 640, 480);
            reducer.scaleFrame(frame.get(), target_size);
        }

        qint64 elapsed = timer.elapsed();
        double ms_per_frame = static_cast<double>(elapsed) / kIterations;
        double fps = 1000.0 / ms_per_frame;

        std::cout << "  " << qualityName(quality) << ": "
                  << ms_per_frame << " ms/frame, "
                  << fps << " fps" << std::endl;
    }
}

TEST(scale_reducer_test, benchmark_partial_update_multi_rect)
{
    const ScaleReducer::Quality qualities[] =
    {
        ScaleReducer::Quality::LOW,
        ScaleReducer::Quality::NORMAL,
        ScaleReducer::Quality::HIGH
    };

    QSize source_size(1920, 1080);
    QSize target_size(960, 540);
    const int kIterations = 2000;

    std::cout << "=== Partial update 4 scattered rects: 1920x1080 -> 960x540 ===" << std::endl;

    for (auto quality : qualities)
    {
        ScaleReducer reducer(quality);
        auto frame = createTestFrame(source_size);
        ASSERT_NE(frame, nullptr);

        reducer.scaleFrame(frame.get(), target_size);

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < kIterations; ++i)
        {
            // Multiple scattered dirty regions.
            *frame->updatedRegion() = QRegion();
            *frame->updatedRegion() += QRect(0, 0, 200, 100);
            *frame->updatedRegion() += QRect(500, 300, 300, 200);
            *frame->updatedRegion() += QRect(1400, 800, 400, 250);
            *frame->updatedRegion() += QRect(100, 900, 150, 150);
            reducer.scaleFrame(frame.get(), target_size);
        }

        qint64 elapsed = timer.elapsed();
        double ms_per_frame = static_cast<double>(elapsed) / kIterations;
        double fps = 1000.0 / ms_per_frame;

        std::cout << "  " << qualityName(quality) << ": "
                  << ms_per_frame << " ms/frame, "
                  << fps << " fps" << std::endl;
    }
}

} // namespace base
