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

#ifndef BASE_AUDIO_AUDIO_CAPTURER_H
#define BASE_AUDIO_AUDIO_CAPTURER_H

#include <QObject>

#include "proto/desktop.h"

#include <functional>
#include <memory>

namespace base {

class AudioCapturer : public QObject
{
    Q_OBJECT

public:
    using PacketCapturedCallback =
        std::function<void(std::unique_ptr<proto::desktop::AudioPacket> packet)>;

    explicit AudioCapturer(QObject* parent)
        : QObject(parent)
    {
        // Nothing
    }
    virtual ~AudioCapturer() = default;

    // Returns true if audio capturing is supported on this platform. If this returns true, then
    // create() must not return nullptr.
    static bool isSupported();
    static std::unique_ptr<AudioCapturer> create();

    static bool isValidSampleRate(int sample_rate);

    // Capturers should sample at a 44.1 or 48 kHz sampling rate, in uncompressed PCM stereo format.
    // Capturers may choose the number of frames per packet. Returns true on success.
    virtual bool start(const PacketCapturedCallback& callback) = 0;
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_CAPTURER_H
