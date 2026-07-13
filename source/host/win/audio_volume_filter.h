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

#ifndef HOST_WIN_AUDIO_VOLUME_FILTER_H
#define HOST_WIN_AUDIO_VOLUME_FILTER_H

#include "host/audio_silence_detector.h"

#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

// A component to modify input audio samples to apply the audio level. Used on Windows, where WASAPI
// loopback returns samples that are not adjusted by the system volume level. This class supports
// frames with 16 bits per sample only.
class AudioVolumeFilter final
{
public:
    // See AudioSilenceDetector for the meaning of |silence_threshold|.
    explicit AudioVolumeFilter(int silence_threshold);
    ~AudioVolumeFilter();

    // Initializes |audio_volume_| from |mm_device|. Returns false if Windows APIs fail.
    bool activateBy(IMMDevice* mm_device);

    // Updates the sampling rate and channels.
    void initialize(int sampling_rate, int channels);

    // Adjusts audio samples in |data|. If the samples are silent before applying the volume level
    // or the audio level is 0, this function returns false. If |frames| is 0, this function also
    // returns false.
    bool apply(qint16* data, size_t frames);

private:
    // Returns the volume level in [0, 1] from |audio_volume_|. If the initialization failed, this
    // function returns 1.
    float audioLevel();

    AudioSilenceDetector silence_detector_;
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> audio_volume_;
};

#endif // HOST_WIN_AUDIO_VOLUME_FILTER_H
