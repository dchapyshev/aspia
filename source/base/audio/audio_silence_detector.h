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

#ifndef BASE_AUDIO_AUDIO_SILENCE_DETECTOR_H
#define BASE_AUDIO_AUDIO_SILENCE_DETECTOR_H

#include <QtGlobal>

namespace base {

// Helper used in audio capturers to detect and drop silent audio packets.
class AudioSilenceDetector
{
public:
    // |threshold| is used to specify maximum absolute sample value that should still be considered
    // as silence.
    explicit AudioSilenceDetector(int threshold);
    ~AudioSilenceDetector();

    void reset(int sampling_rate, int channels);

    // Must be called for each new chunk of data. Return true the given packet is silence should be
    // dropped.
    bool isSilence(const qint16* samples, size_t frames);

    // The count of channels received from last Reset().
    int channels() const;

private:
    // Maximum absolute sample value that should still be considered as silence.
    int threshold_;

    // Silence period threshold in samples. Silence intervals shorter than this value are still
    // encoded and sent to the client, so that we don't disrupt playback by dropping them.
    int silence_length_max_;

    // Lengths of the current silence period in samples.
    int silence_length_;

    // The count of channels.
    int channels_;
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_SILENCE_DETECTOR_H
