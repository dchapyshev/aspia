//
// Aspia Project
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

#include "base/audio/audio_volume_filter.h"

namespace base {

//--------------------------------------------------------------------------------------------------
AudioVolumeFilter::AudioVolumeFilter(int silence_threshold)
    : silence_detector_(silence_threshold)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
AudioVolumeFilter::~AudioVolumeFilter() = default;

//--------------------------------------------------------------------------------------------------
bool AudioVolumeFilter::apply(qint16* data, size_t frames)
{
    if (frames == 0)
        return false;

    if (silence_detector_.isSilence(data, frames))
        return false;

    float level = audioLevel();
    if (level == 0)
        return false;

    if (level == 1)
        return true;

    const int sample_count = static_cast<int>(frames) * silence_detector_.channels();
    const qint32 level_int = static_cast<qint32>(level * 65536);

    for (int i = 0; i < sample_count; i++)
    {
        data[i] = (static_cast<qint32>(data[i]) * level_int) >> 16;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioVolumeFilter::initialize(int sampling_rate, int channels)
{
    silence_detector_.reset(sampling_rate, channels);
}

} // namespace base
