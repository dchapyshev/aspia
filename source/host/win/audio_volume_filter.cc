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

#include "host/win/audio_volume_filter.h"

#include <comdef.h>

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
AudioVolumeFilter::AudioVolumeFilter(int silence_threshold)
    : silence_detector_(silence_threshold)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
AudioVolumeFilter::~AudioVolumeFilter() = default;

//--------------------------------------------------------------------------------------------------
bool AudioVolumeFilter::activateBy(IMMDevice* mm_device)
{
    DCHECK(mm_device);
    audio_volume_.Reset();
    // TODO(zijiehe): Do we need to control the volume per process?
    _com_error hr = mm_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
        nullptr, &audio_volume_);
    if (FAILED(hr.Error()))
    {
        LOG(ERROR) << "Failed to get an IAudioEndpointVolume:" << hr;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioVolumeFilter::initialize(int sampling_rate, int channels)
{
    silence_detector_.reset(sampling_rate, channels);
}

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
float AudioVolumeFilter::audioLevel()
{
    if (!audio_volume_)
        return 1;

    BOOL mute;
    _com_error hr = audio_volume_->GetMute(&mute);
    if (FAILED(hr.Error()))
    {
        LOG(ERROR) << "Failed to get mute status from IAudioEndpointVolume:" << hr;
        return 1;
    }

    if (mute)
        return 0;

    float level;
    hr = audio_volume_->GetMasterVolumeLevelScalar(&level);
    if (FAILED(hr.Error()) || level > 1)
    {
        LOG(ERROR) << "Failed to get master volume from IAudioEndpointVolume:" << hr;
        return 1;
    }

    if (level < 0)
        return 0;

    return level;
}
