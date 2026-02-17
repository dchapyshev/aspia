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

#ifndef BASE_AUDIO_AUDIO_VOLUME_FILER_WIN_H
#define BASE_AUDIO_AUDIO_VOLUME_FILER_WIN_H

#include "base/audio/audio_volume_filter.h"

#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace base {

// An implementation of AudioVolumeFilter for Windows only.
class AudioVolumeFilterWin final : public AudioVolumeFilter
{
public:
    explicit AudioVolumeFilterWin(int silence_threshold);
    ~AudioVolumeFilterWin() final;

    // Initializes |audio_volume_|. Returns false if Windows APIs fail.
    bool activateBy(IMMDevice* mm_device);

protected:
    // Returns current audio level from |audio_volume_|. If the initialization failed, this
    // function returns 1.
    float audioLevel() final;

private:
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> audio_volume_;
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_VOLUME_FILER_WIN_H
