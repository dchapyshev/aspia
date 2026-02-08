//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/audio/audio_silence_detector.h"

#include "base/logging.h"

namespace base {

namespace {

// Silence period threshold in seconds. Silence intervals shorter than this value are still encoded
// and sent to the client, so that we don't disrupt playback by dropping them.
int kSilencePeriodThresholdSeconds = 1;

}  // namespace

//--------------------------------------------------------------------------------------------------
AudioSilenceDetector::AudioSilenceDetector(int threshold)
    : threshold_(threshold),
      silence_length_max_(0),
      silence_length_(0),
      channels_(0)
{
    DCHECK_GE(threshold_, 0);
}

//--------------------------------------------------------------------------------------------------
AudioSilenceDetector::~AudioSilenceDetector() = default;

//--------------------------------------------------------------------------------------------------
void AudioSilenceDetector::reset(int sampling_rate, int channels)
{
    DCHECK_GT(sampling_rate, 0);
    DCHECK_GT(channels, 0);

    silence_length_ = 0;
    silence_length_max_ = sampling_rate * channels * kSilencePeriodThresholdSeconds;
    channels_ = channels;
}

//--------------------------------------------------------------------------------------------------
bool AudioSilenceDetector::isSilence(const qint16* samples, size_t frames)
{
    const int samples_count = static_cast<int>(frames) * channels();
    bool silent_packet = true;
    // Potentially this loop can be optimized (e.g. using SSE or adding special case for
    // threshold_==0), but it's not worth worrying about because the amount of data it processes is
    // relaively small.
    for (int i = 0; i < samples_count; ++i)
    {
        if (abs(samples[i]) > threshold_)
        {
            silent_packet = false;
            break;
        }
    }

    if (!silent_packet)
    {
        silence_length_ = 0;
        return false;
    }

    silence_length_ += samples_count;
    return silence_length_ > silence_length_max_;
}

//--------------------------------------------------------------------------------------------------
int AudioSilenceDetector::channels() const
{
    return channels_;
}

} // namespace base
