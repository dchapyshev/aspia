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

#ifndef BASE_AUDIO_AUDIO_OUTPUT_PULSE_H
#define BASE_AUDIO_AUDIO_OUTPUT_PULSE_H

#include <QTimer>

#include "base/audio/audio_output.h"
#include "base/audio/linux/pulseaudio_symbol_table.h"

#include <mutex>

#include <asio/high_resolution_timer.hpp>
#include <pulse/pulseaudio.h>

namespace base {

class SimpleThread;

class AudioOutputPulse final : public AudioOutput
{
public:
    explicit AudioOutputPulse(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputPulse();

    // AudioOutput implementation.
    bool start() final;
    bool stop() final;

private:
    static void paContextStateCallback(pa_context* context, void* self);
    static void paServerInfoCallback(pa_context* context, const pa_server_info* info, void* self);
    static void paStreamStateCallback(pa_stream* stream, void* self);
    static void paStreamWriteCallback(pa_stream* stream, size_t buffer_space, void* self);

    void paContextStateCallbackHandler(pa_context* context);
    void paServerInfoCallbackHandler(const pa_server_info* info);
    void paStreamStateCallbackHandler(pa_stream* stream);
    void paStreamWriteCallbackHandler();

    bool initDevice();
    bool initPlayout();
    void terminate();
    bool initPulseAudio();
    void terminatePulseAudio();
    void onTimerExpired();
    void writePlayoutData();

    bool device_initialized_ = false;
    bool playing_ = false;
    bool playout_initialized_ = false;
    bool start_play_ = false;

    bool pa_state_changed_ = false;
    pa_threaded_mainloop* pa_main_loop_ = nullptr;
    pa_mainloop_api* pa_main_loop_api_ = nullptr;
    pa_context* pa_context_ = nullptr;

    pa_stream* play_stream_ = nullptr;
    QTimer* timer_ = nullptr;

    int period_time_ = 0;
    int period_size_ = 0;
    int buffer_size_ = 0;
    int max_buffer_size_ = 0;

    std::unique_ptr<char[]> play_buffer_;

    Q_DISABLE_COPY(AudioOutputPulse)
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_OUTPUT_PULSE_H
