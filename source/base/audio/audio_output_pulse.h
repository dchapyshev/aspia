//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__AUDIO__AUDIO_OUTPUT_PULSE_H
#define BASE__AUDIO__AUDIO_OUTPUT_PULSE_H

#include "base/waitable_event.h"
#include "base/audio/audio_output.h"
#include "base/audio/linux/pulseaudio_symbol_table.h"

#include <mutex>

#include <pulse/pulseaudio.h>

namespace base {

class SimpleThread;

class AudioOutputPulse : public AudioOutput
{
public:
    explicit AudioOutputPulse(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputPulse();

    // AudioOutput implementation.
    bool start() override;
    bool stop() override;

private:
    static void paContextStateCallback(pa_context* c, void* self);
    static void paServerInfoCallback(pa_context* c, const pa_server_info* i, void* self);
    static void paStreamStateCallback(pa_stream* p, void* self);
    static void paStreamWriteCallback(pa_stream* unused, size_t buffer_space, void* self);

    void paContextStateCallbackHandler(pa_context* c);
    void paServerInfoCallbackHandler(const pa_server_info* i);
    void paStreamStateCallbackHandler(pa_stream* p);
    void paStreamWriteCallbackHandler(size_t buffer_space);

    void enableWriteCallback();
    void disableWriteCallback();

    bool initDevice();
    bool initPlayout();
    void terminate();
    bool initPulseAudio();
    void terminatePulseAudio();
    void threadRun();

    std::mutex mutex_;
    WaitableEvent time_event_play_;
    WaitableEvent play_start_event_;

    std::unique_ptr<SimpleThread> worker_thread_;

    bool device_initialized_ = false;
    bool playing_ = false;
    bool playout_initialized_ = false;
    bool start_play_ = false;
    bool quit_ = false;

    int32_t write_errors_ = 0;

    std::unique_ptr<int8_t[]> play_buffer_;
    size_t playback_buffer_size_ = 0;
    size_t playback_buffer_unused_ = 0;
    size_t temp_buffer_space_ = 0;

    bool pa_state_changed_ = false;
    pa_threaded_mainloop* pa_main_loop_ = nullptr;
    pa_mainloop_api* pa_main_loop_api_ = nullptr;
    pa_context* pa_context_ = nullptr;

    pa_stream* play_stream_ = nullptr;
    uint32_t play_stream_flags_ = 0;
    pa_buffer_attr play_buffer_attr_;

    DISALLOW_COPY_AND_ASSIGN(AudioOutputPulse);
};

} // namespace base

#endif // BASE__AUDIO__AUDIO_OUTPUT_PULSE_H
