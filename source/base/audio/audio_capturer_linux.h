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

#ifndef BASE_AUDIO_AUDIO_CAPTURER_LINUX_H
#define BASE_AUDIO_AUDIO_CAPTURER_LINUX_H

#include "base/audio/audio_capturer.h"
#include "base/audio/audio_silence_detector.h"
#include "base/audio/linux/audio_pipe_reader.h"

namespace base {

class AudioCapturerLinux final
    : public AudioCapturer,
      public AudioPipeReader::Delegate
{
public:
    AudioCapturerLinux();
    ~AudioCapturerLinux() final;

    // AudioCapturer interface.
    bool start(const PacketCapturedCallback& callback) final;

protected:
    // AudioPipeReader::Delegate implementation.
    void onDataRead(const std::string& data) final;

private:
    local_shared_ptr<AudioPipeReader> pipe_reader_;
    PacketCapturedCallback callback_;
    AudioSilenceDetector silence_detector_;

    Q_DISABLE_COPY(AudioCapturerLinux)
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_CAPTURER_LINUX_H
