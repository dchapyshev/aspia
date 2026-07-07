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

#ifndef BASE_AUDIO_AUDIO_CAPTURER_MAC_H
#define BASE_AUDIO_AUDIO_CAPTURER_MAC_H

#include "base/audio/audio_capturer.h"

#include <memory>

struct AudioCapturerMacImpl;

// Captures the system audio (what applications play, not the microphone) through ScreenCaptureKit:
// an audio-only SCStream delivers PCM packets from its dispatch queue. Requires macOS 13; the same
// Screen Recording permission as the video capture covers it, no extra prompt appears. If the
// stream stops on its own (display unplugged, permission revoked), the capture restarts
// automatically.
class AudioCapturerMac final : public AudioCapturer
{
    Q_OBJECT

public:
    explicit AudioCapturerMac(QObject* parent = nullptr);
    ~AudioCapturerMac() final;

    // AudioCapturer interface.
    bool start(const PacketCapturedCallback& callback) final;

private slots:
    void onStreamStopped();

private:
    bool startStream();
    void stopStream();

    std::unique_ptr<AudioCapturerMacImpl> impl_;
    int restart_attempts_ = 0;

    Q_DISABLE_COPY(AudioCapturerMac)
};

#endif // BASE_AUDIO_AUDIO_CAPTURER_MAC_H
