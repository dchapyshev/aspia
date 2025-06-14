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

#include "base/audio/audio_volume_filter_win.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
AudioVolumeFilterWin::AudioVolumeFilterWin(int silence_threshold)
    : AudioVolumeFilter(silence_threshold)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
AudioVolumeFilterWin::~AudioVolumeFilterWin() = default;

//--------------------------------------------------------------------------------------------------
bool AudioVolumeFilterWin::activateBy(IMMDevice* mm_device)
{
    DCHECK(mm_device);
    audio_volume_.Reset();
    // TODO(zijiehe): Do we need to control the volume per process?
    HRESULT hr = mm_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
        nullptr, &audio_volume_);
    if (FAILED(hr))
    {
        LOG(ERROR) << "Failed to get an IAudioEndpointVolume. Error" << hr;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
float AudioVolumeFilterWin::audioLevel()
{
    if (!audio_volume_)
        return 1;

    BOOL mute;
    HRESULT hr = audio_volume_->GetMute(&mute);
    if (FAILED(hr))
    {
        LOG(ERROR) << "Failed to get mute status from IAudioEndpointVolume, error" << hr;
        return 1;
    }

    if (mute)
        return 0;

    float level;
    hr = audio_volume_->GetMasterVolumeLevelScalar(&level);
    if (FAILED(hr) || level > 1)
    {
        LOG(ERROR) << "Failed to get master volume from IAudioEndpointVolume, error" << hr;
        return 1;
    }

    if (level < 0)
        return 0;

    return level;
}

} // namespace base
