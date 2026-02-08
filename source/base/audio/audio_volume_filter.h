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

#ifndef BASE_AUDIO_AUDIO_VOLUME_FILTER_H
#define BASE_AUDIO_AUDIO_VOLUME_FILTER_H

#include "base/audio/audio_silence_detector.h"

namespace base {

// A component to modify input audio sample to apply the audio level. This class is used on
// platforms which returns non-adjusted audio samples, e.g. Windows.
// This class supports frames with 16 bits per sample only.
class AudioVolumeFilter
{
public:
    // See AudioSilenceDetector for the meaning of |silence_threshold|.
    explicit AudioVolumeFilter(int silence_threshold);
    virtual ~AudioVolumeFilter();

    // Adjusts audio samples in |data|. If the samples are silent before applying the volume level
    // or the audioLevel() returns 0, this function returns false. If |frames| is 0, this
    // function also returns false.
    bool apply(qint16* data, size_t frames);

    // Updates the sampling rate and channels.
    void initialize(int sampling_rate, int channels);

protected:
    // Returns the volume level in [0, 1]. This should be a normalized scalar value: sample values
    // can be simply multiplied by the result of this function to apply volume.
    virtual float audioLevel() = 0;

private:
    AudioSilenceDetector silence_detector_;
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_VOLUME_FILTER_H
