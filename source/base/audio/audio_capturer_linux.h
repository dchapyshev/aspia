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

#ifndef BASE_AUDIO_AUDIO_CAPTURER_LINUX_H
#define BASE_AUDIO_AUDIO_CAPTURER_LINUX_H

#include "base/audio/audio_capturer.h"
#include "base/audio/audio_silence_detector.h"
#include "base/audio/linux/pulseaudio_symbol_table.h"

#include <pulse/pulseaudio.h>

#include <string>

// Captures the system audio (what applications play, not the microphone) through PulseAudio: a record
// stream connected to the monitor source of the default sink. Packets are delivered from the PulseAudio
// mainloop thread; the default sink is tracked through server change events, so the capture follows
// when the output device changes (e.g. headphones are plugged in).
class AudioCapturerLinux final : public AudioCapturer
{
public:
    explicit AudioCapturerLinux(QObject* parent = nullptr);
    ~AudioCapturerLinux() final;

    // AudioCapturer interface.
    bool start(const PacketCapturedCallback& callback) final;

private:
    static void paContextStateCallback(pa_context* context, void* self);
    static void paServerInfoCallback(pa_context* context, const pa_server_info* info, void* self);
    static void paSubscribeCallback(pa_context* context, pa_subscription_event_type_t type,
                                    uint32_t index, void* self);
    static void paStreamStateCallback(pa_stream* stream, void* self);
    static void paStreamReadCallback(pa_stream* stream, size_t bytes, void* self);

    void paContextStateCallbackHandler(pa_context* context);
    void paServerInfoCallbackHandler(const pa_server_info* info);
    void paStreamStateCallbackHandler(pa_stream* stream);
    void paStreamReadCallbackHandler(pa_stream* stream);

    bool initPulseAudio();
    void terminatePulseAudio();

    // Stream management. Both are called either with the mainloop lock held (start/stop paths) or
    // from a callback on the mainloop thread (default sink change), where the lock is already held.
    bool connectStream();
    void disconnectStream();

    // Builds a packet from a chunk of recorded PCM and hands it to the callback; silent chunks are
    // dropped after a grace period.
    void processChunk(const void* data, size_t bytes);

    PacketCapturedCallback callback_;
    AudioSilenceDetector silence_detector_;

    bool pa_state_changed_ = false;
    pa_threaded_mainloop* pa_main_loop_ = nullptr;
    pa_mainloop_api* pa_main_loop_api_ = nullptr;
    pa_context* pa_context_ = nullptr;
    pa_stream* stream_ = nullptr;

    // The sink whose monitor is being captured; server change events compare against it to detect a
    // default sink switch.
    std::string default_sink_;

    Q_DISABLE_COPY(AudioCapturerLinux)
};

#endif // BASE_AUDIO_AUDIO_CAPTURER_LINUX_H
