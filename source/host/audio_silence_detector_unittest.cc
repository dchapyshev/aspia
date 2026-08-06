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

#include "host/audio_silence_detector.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

const int kSamplingRate = 100;
const int kChannels = 2;

// One second of audio at the rate above; the detector starts dropping packets past this much
// silence.
const size_t kFramesPerSecond = kSamplingRate;

std::vector<qint16> samples(size_t frames, qint16 value)
{
    return std::vector<qint16>(frames * kChannels, value);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(AudioSilenceDetectorTest, ChannelsComeFromReset)
{
    AudioSilenceDetector detector(0);
    EXPECT_EQ(detector.channels(), 0);

    detector.reset(kSamplingRate, kChannels);
    EXPECT_EQ(detector.channels(), kChannels);

    detector.reset(kSamplingRate, 1);
    EXPECT_EQ(detector.channels(), 1);
}

//--------------------------------------------------------------------------------------------------
// Silence shorter than a second is still sent, so that playback is not disrupted by dropping it.
TEST(AudioSilenceDetectorTest, ShortSilenceIsNotDropped)
{
    AudioSilenceDetector detector(0);
    detector.reset(kSamplingRate, kChannels);

    const std::vector<qint16> silence = samples(kFramesPerSecond / 2, 0);

    EXPECT_FALSE(detector.isSilence(silence.data(), kFramesPerSecond / 2));
    EXPECT_FALSE(detector.isSilence(silence.data(), kFramesPerSecond / 2));
}

//--------------------------------------------------------------------------------------------------
TEST(AudioSilenceDetectorTest, LongSilenceIsDropped)
{
    AudioSilenceDetector detector(0);
    detector.reset(kSamplingRate, kChannels);

    const std::vector<qint16> silence = samples(kFramesPerSecond, 0);

    // The first second of silence is still sent, everything past it is dropped.
    EXPECT_FALSE(detector.isSilence(silence.data(), kFramesPerSecond));
    EXPECT_TRUE(detector.isSilence(silence.data(), kFramesPerSecond));
    EXPECT_TRUE(detector.isSilence(silence.data(), kFramesPerSecond));
}

//--------------------------------------------------------------------------------------------------
TEST(AudioSilenceDetectorTest, SoundRestartsTheSilencePeriod)
{
    AudioSilenceDetector detector(0);
    detector.reset(kSamplingRate, kChannels);

    const std::vector<qint16> silence = samples(kFramesPerSecond, 0);
    const std::vector<qint16> sound = samples(kFramesPerSecond, 1000);

    EXPECT_FALSE(detector.isSilence(silence.data(), kFramesPerSecond));
    ASSERT_TRUE(detector.isSilence(silence.data(), kFramesPerSecond));

    EXPECT_FALSE(detector.isSilence(sound.data(), kFramesPerSecond));

    // The counter starts over, so the silence that follows is sent again.
    EXPECT_FALSE(detector.isSilence(silence.data(), kFramesPerSecond));
}

//--------------------------------------------------------------------------------------------------
TEST(AudioSilenceDetectorTest, ResetStartsTheSilencePeriodOver)
{
    AudioSilenceDetector detector(0);
    detector.reset(kSamplingRate, kChannels);

    const std::vector<qint16> silence = samples(kFramesPerSecond, 0);

    EXPECT_FALSE(detector.isSilence(silence.data(), kFramesPerSecond));
    ASSERT_TRUE(detector.isSilence(silence.data(), kFramesPerSecond));

    detector.reset(kSamplingRate, kChannels);
    EXPECT_FALSE(detector.isSilence(silence.data(), kFramesPerSecond));
}

//--------------------------------------------------------------------------------------------------
// Anything up to the threshold counts as silence; the first sample past it makes the whole packet
// sound.
TEST(AudioSilenceDetectorTest, ThresholdIsInclusive)
{
    AudioSilenceDetector detector(10);
    detector.reset(kSamplingRate, kChannels);

    std::vector<qint16> quiet = samples(kFramesPerSecond, 10);

    EXPECT_FALSE(detector.isSilence(quiet.data(), kFramesPerSecond));
    EXPECT_TRUE(detector.isSilence(quiet.data(), kFramesPerSecond));

    quiet[quiet.size() - 1] = 11;
    EXPECT_FALSE(detector.isSilence(quiet.data(), kFramesPerSecond));
}

//--------------------------------------------------------------------------------------------------
TEST(AudioSilenceDetectorTest, NegativeSamplesUseAbsoluteValue)
{
    AudioSilenceDetector detector(10);
    detector.reset(kSamplingRate, kChannels);

    std::vector<qint16> quiet = samples(kFramesPerSecond, -10);

    EXPECT_FALSE(detector.isSilence(quiet.data(), kFramesPerSecond));
    EXPECT_TRUE(detector.isSilence(quiet.data(), kFramesPerSecond));

    quiet[0] = -11;
    EXPECT_FALSE(detector.isSilence(quiet.data(), kFramesPerSecond));
}
