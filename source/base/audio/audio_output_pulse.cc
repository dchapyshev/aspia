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

#include "base/audio/audio_output_pulse.h"

#include "base/logging.h"
#include "base/threading/simple_thread.h"

base::PulseAudioSymbolTable* pulseSymbolTable()
{
    static base::PulseAudioSymbolTable* pulse_symbol_table = new base::PulseAudioSymbolTable();
    return pulse_symbol_table;
}

// Accesses Pulse functions through our late-binding symbol table instead of directly. This way we
// don't have to link to libpulse, which means our binary will work on systems that don't have it.
#define LATE(sym) LATESYM_GET(base::PulseAudioSymbolTable, pulseSymbolTable(), sym)

namespace base {

namespace {

// First PulseAudio protocol version that supports PA_STREAM_ADJUST_LATENCY.
const uint32_t kPaAdjustLatencyProtocolVersion = 13;

// For playback, there is a round-trip delay to fill the server-side playback buffer, so setting too
// low of a latency is a buffer underflow risk. We will automatically increase the latency if a
// buffer underflow does occur, but we also enforce a sane minimum at start-up time. Anything lower
// would be virtually guaranteed to underflow at least once, so there's no point in allowing lower
// latencies.
const uint32_t kPaPlaybackLatencyMinMSecs = 20;

// Every time a playback stream underflows, we will reconfigure it with target latency that is
// greater by this amount.
const uint32_t kPaPlaybackLatencyIncrementMSecs = 20;

// We also need to configure a suitable request size. Too small and we'd burn CPU from the overhead
// of transfering small amounts of data at once. Too large and the amount of data remaining in the
// buffer right before refilling it would be a buffer underflow risk. We set it to half of the
// buffer size.
const uint32_t kPaPlaybackRequestFactor = 2;

class ScopedPaLock
{
public:
    explicit ScopedPaLock(pa_threaded_mainloop* pa_main_loop)
        : pa_main_loop_(pa_main_loop)
    {
        LATE(pa_threaded_mainloop_lock)(pa_main_loop_);
    }

    ~ScopedPaLock()
    {
        LATE(pa_threaded_mainloop_unlock)(pa_main_loop_);
    }

private:
    pa_threaded_mainloop* pa_main_loop_;
    DISALLOW_COPY_AND_ASSIGN(ScopedPaLock);
};

} // namespace

AudioOutputPulse::AudioOutputPulse(const NeedMoreDataCB& need_more_data_cb)
    : AudioOutput(need_more_data_cb)
{
    memset(&play_buffer_attr_, 0, sizeof(play_buffer_attr_));

    if (initDevice())
        initPlayout();
}

AudioOutputPulse::~AudioOutputPulse()
{
    stop();
    terminate();
}

bool AudioOutputPulse::start()
{
    if (!playout_initialized_)
        return false;

    if (playing_)
        return true;

    // Set state to ensure that playout starts from the audio thread.
    {
        std::scoped_lock lock(mutex_);
        start_play_ = true;
    }

    // Both |_startPlay| and |_playing| needs protction since they are also accessed on the playout
    // thread.

    // The audio thread will signal when playout has started.
    time_event_play_.signal();
    if (!play_start_event_.wait(std::chrono::milliseconds(10000)))
    {
        {
            std::scoped_lock lock(mutex_);
            start_play_ = false;
        }
        stop();
        LOG(LS_ERROR) << "Failed to activate playout";
        return false;
    }

    {
        std::scoped_lock lock(mutex_);
        if (playing_)
        {
            // The playing state is set by the audio thread after playout has started.
        }
        else
        {
            LOG(LS_ERROR) << "Failed to activate playing";
            return false;
        }
    }

    LOG(LS_INFO) << "Audio playout started";
    return true;
}

bool AudioOutputPulse::stop()
{
    std::scoped_lock lock(mutex_);

    if (!playout_initialized_)
        return true;

    if (!play_stream_)
        return false;

    playout_initialized_ = false;
    playing_ = false;

    {
        ScopedPaLock pa_lock(pa_main_loop_);

        disableWriteCallback();
        LATE(pa_stream_set_underflow_callback)(play_stream_, nullptr, nullptr);

        // Unset this here so that we don't get a TERMINATED callback.
        LATE(pa_stream_set_state_callback)(play_stream_, nullptr, nullptr);

        if (LATE(pa_stream_get_state)(play_stream_) != PA_STREAM_UNCONNECTED)
        {
            // Disconnect the stream.
            if (LATE(pa_stream_disconnect)(play_stream_) != PA_OK)
            {
                LOG(LS_ERROR) << "Failed to disconnect play stream: "
                              << LATE(pa_context_errno)(pa_context_);
                return false;
            }

            LOG(LS_INFO) << "Disconnected playback";
        }

        LATE(pa_stream_unref)(play_stream_);
        play_stream_ = nullptr;
    }

    play_buffer_.reset();

    LOG(LS_INFO) << "Audio playout stopped";
    return true;
}

// static
void AudioOutputPulse::paContextStateCallback(pa_context* c, void* self)
{
    static_cast<AudioOutputPulse*>(self)->paContextStateCallbackHandler(c);
}

// static
void AudioOutputPulse::paServerInfoCallback(pa_context*, const pa_server_info* i, void* self)
{
    static_cast<AudioOutputPulse*>(self)->paServerInfoCallbackHandler(i);
}

// static
void AudioOutputPulse::paStreamStateCallback(pa_stream* p, void* self)
{
    static_cast<AudioOutputPulse*>(self)->paStreamStateCallbackHandler(p);
}

// static
void AudioOutputPulse::paStreamWriteCallback(pa_stream*, size_t buffer_space, void* self)
{
    static_cast<AudioOutputPulse*>(self)->paStreamWriteCallbackHandler(buffer_space);
}

// static
void AudioOutputPulse::paStreamUnderflowCallback(pa_stream*, void* self)
{
    static_cast<AudioOutputPulse*>(self)->paStreamUnderflowCallbackHandler();
}

void AudioOutputPulse::paContextStateCallbackHandler(pa_context* c)
{
    pa_context_state_t state = LATE(pa_context_get_state)(c);
    switch (state)
    {
        case PA_CONTEXT_UNCONNECTED:
            break;

        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_state_changed_ = true;
            LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
            break;

        case PA_CONTEXT_READY:
            pa_state_changed_ = true;
            LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
            break;
    }
}

void AudioOutputPulse::paServerInfoCallbackHandler(const pa_server_info* i)
{
    LOG(LS_INFO) << "PulseAudio version: " << i->server_version;
    LOG(LS_INFO) << "Native sample rate: " << i->sample_spec.rate;
    LOG(LS_INFO) << "Native channels: " << i->sample_spec.channels;
    LOG(LS_INFO) << "Native format: " << i->sample_spec.format;

    LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
}

void AudioOutputPulse::paStreamStateCallbackHandler(pa_stream*)
{
    LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
}

void AudioOutputPulse::paStreamWriteCallbackHandler(size_t buffer_space)
{
    temp_buffer_space_ = buffer_space;

    // Since we write the data asynchronously on a different thread, we have to temporarily disable
    // the write callback or else Pulse will call it continuously until we write the data. We
    // re-enable it below.
    disableWriteCallback();
    time_event_play_.signal();
}

void AudioOutputPulse::paStreamUnderflowCallbackHandler()
{
    const pa_sample_spec* spec = LATE(pa_stream_get_sample_spec)(play_stream_);
    if (!spec)
    {
        LOG(LS_ERROR) << "pa_stream_get_sample_spec failed";
        return;
    }

    size_t bytes_per_sec = LATE(pa_bytes_per_second)(spec);
    uint32_t new_latency =
        configured_latency_ + bytes_per_sec * kPaPlaybackLatencyIncrementMSecs / 1000;

    // Set the play buffer attributes.
    play_buffer_attr_.maxlength = new_latency;
    play_buffer_attr_.tlength = new_latency;
    play_buffer_attr_.minreq = new_latency / kPaPlaybackRequestFactor;
    play_buffer_attr_.prebuf = play_buffer_attr_.tlength - play_buffer_attr_.minreq;

    pa_operation* op = LATE(pa_stream_set_buffer_attr)(
        play_stream_, &play_buffer_attr_, nullptr, nullptr);
    if (!op)
    {
        LOG(LS_ERROR) << "pa_stream_set_buffer_attr failed";
        return;
    }

    // Don't need to wait for this to complete.
    LATE(pa_operation_unref)(op);

    // Save the new latency in case we underflow again.
    configured_latency_ = new_latency;
}

void AudioOutputPulse::enableWriteCallback()
{
    if (LATE(pa_stream_get_state)(play_stream_) == PA_STREAM_READY)
    {
        // May already have available space. Must check.
        temp_buffer_space_ = LATE(pa_stream_writable_size)(play_stream_);
        if (temp_buffer_space_ > 0)
        {
            // Yup, there is already space available, so if we register a write callback then it
            // will not receive any event. So dispatch one ourself instead.
            time_event_play_.signal();
            return;
        }
    }

    LATE(pa_stream_set_write_callback)(play_stream_, &paStreamWriteCallback, this);
}

void AudioOutputPulse::disableWriteCallback()
{
    LATE(pa_stream_set_write_callback)(play_stream_, nullptr, nullptr);
}

bool AudioOutputPulse::initDevice()
{
    if (device_initialized_)
        return true;

    // Initialize PulseAudio.
    if (!initPulseAudio())
    {
        LOG(LS_ERROR) << "Failed to initialize PulseAudio";
        terminatePulseAudio();
        return false;
    }

    worker_thread_ = std::make_unique<SimpleThread>();
    worker_thread_->start(std::bind(&AudioOutputPulse::threadRun, this));

    LOG(LS_INFO) << "Audio device initialized";
    device_initialized_ = true;
    return true;
}

bool AudioOutputPulse::initPlayout()
{
    if (playing_)
        return false;

    if (playout_initialized_)
        return true;

    // Set the play sample specification.
    pa_sample_spec play_sample_spec;
    play_sample_spec.channels = kChannels;
    play_sample_spec.format = PA_SAMPLE_S16LE;
    play_sample_spec.rate = kSampleRate;

    // Create a new play stream.
    {
        std::scoped_lock lock(mutex_);
        play_stream_ = LATE(pa_stream_new)(pa_context_, "playStream", &play_sample_spec, nullptr);
    }

    if (!play_stream_)
    {
        LOG(LS_ERROR) << "Failed to create play stream: " << LATE(pa_context_errno)(pa_context_);
        return false;
    }

    // Set stream flags.
    play_stream_flags_ = static_cast<pa_stream_flags_t>(
         PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING);

    // If configuring a specific latency then we want to specify PA_STREAM_ADJUST_LATENCY to make
    // the server adjust parameters automatically to reach that target latency. However, that flag
    // doesn't exist in Ubuntu 8.04 and many people still use that, so we have to check the protocol
    // version of libpulse.
    if (LATE(pa_context_get_protocol_version)(pa_context_) >= kPaAdjustLatencyProtocolVersion)
    {
        play_stream_flags_ |= PA_STREAM_ADJUST_LATENCY;
    }

    const pa_sample_spec* spec = LATE(pa_stream_get_sample_spec)(play_stream_);
    if (!spec)
    {
        LOG(LS_ERROR) << "pa_stream_get_sample_spec failed";
        return false;
    }

    size_t bytes_per_sec = LATE(pa_bytes_per_second)(spec);
    uint32_t latency = bytes_per_sec * kPaPlaybackLatencyMinMSecs / 1000;

    // Set the play buffer attributes.
    play_buffer_attr_.maxlength = latency;  // Number bytes stored in the buffer.
    play_buffer_attr_.tlength = latency;    // Target fill level of play buffer.
    // Minimum free num bytes before server request more data.
    play_buffer_attr_.minreq = latency / kPaPlaybackRequestFactor;
    // prebuffer tlength before starting playout
    play_buffer_attr_.prebuf = play_buffer_attr_.tlength - play_buffer_attr_.minreq;
    configured_latency_ = latency;

    // Num samples in bytes * num channels.
    playback_buffer_size_ = kSampleRate / 100 * 2 * kChannels;
    playback_buffer_unused_ = playback_buffer_size_;
    play_buffer_ = std::make_unique<int8_t[]>(playback_buffer_size_);

    // Enable underflow callback.
    LATE(pa_stream_set_underflow_callback)(play_stream_, paStreamUnderflowCallback, this);

    // Set the state callback function for the stream.
    LATE(pa_stream_set_state_callback)(play_stream_, paStreamStateCallback, this);

    // Mark playout side as initialized.
    {
        std::scoped_lock lock(mutex_);
        playout_initialized_ = true;
    }

    LOG(LS_INFO) << "Audio playout initialized";
    return true;
}

void AudioOutputPulse::terminate()
{
    if (!device_initialized_)
        return;

    {
        std::scoped_lock lock(mutex_);
        quit_ = true;
    }

    if (worker_thread_)
    {
        time_event_play_.signal();
        worker_thread_->stop();
        worker_thread_.reset();
    }

    // Terminate PulseAudio.
    terminatePulseAudio();

    LOG(LS_INFO) << "Audio output terminated";
    device_initialized_ = false;
}

bool AudioOutputPulse::initPulseAudio()
{
    if (!pulseSymbolTable()->load())
    {
        // Most likely the Pulse library and sound server are not installed on this system.
        LOG(LS_ERROR) << "Failed to load symbol table";
        return false;
    }

    // Create a mainloop API and connection to the default server the mainloop is the internal
    // asynchronous API event loop.
    if (pa_main_loop_)
    {
        LOG(LS_ERROR) << "PA mainloop has already existed";
        return false;
    }

    pa_main_loop_ = LATE(pa_threaded_mainloop_new)();
    if (!pa_main_loop_)
    {
        LOG(LS_ERROR) << "Could not create mainloop";
        return false;
    }

    // Start the threaded main loop.
    int ret = LATE(pa_threaded_mainloop_start)(pa_main_loop_);
    if (ret != PA_OK)
    {
        LOG(LS_ERROR) << "Failed to start main loop: " << ret;
        return false;
    }

    ScopedPaLock pa_lock(pa_main_loop_);

    pa_main_loop_api_ = LATE(pa_threaded_mainloop_get_api)(pa_main_loop_);
    if (!pa_main_loop_api_)
    {
        LOG(LS_ERROR) << "Could not create mainloop API";
        return false;
    }

    if (pa_context_)
    {
        LOG(LS_ERROR) << "PA context has already existed";
        return false;
    }

    // Create a new PulseAudio context.
    pa_context_ = LATE(pa_context_new)(pa_main_loop_api_, "Aspia Client");
    if (!pa_context_)
    {
        LOG(LS_ERROR) << "Could not create context";
        return false;
    }

    // Set state callback function.
    LATE(pa_context_set_state_callback)(pa_context_, paContextStateCallback, this);

    // Connect the context to a server (default).
    pa_state_changed_ = false;
    ret = LATE(pa_context_connect)(pa_context_, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);
    if (ret != PA_OK)
    {
        LOG(LS_ERROR) << "Failed to connect context: " << ret;
        return false;
    }

    // Wait for state change.
    while (!pa_state_changed_)
        LATE(pa_threaded_mainloop_wait)(pa_main_loop_);

    // Now check to see what final state we reached.
    pa_context_state_t state = LATE(pa_context_get_state)(pa_context_);
    if (state != PA_CONTEXT_READY)
    {
        if (state == PA_CONTEXT_FAILED)
        {
            LOG(LS_ERROR) << "Failed to connect to PulseAudio sound server";
        }
        else if (state == PA_CONTEXT_TERMINATED)
        {
            LOG(LS_ERROR) << "PulseAudio connection terminated early";
        }
        else
        {
            // Shouldn't happen, because we only signal on one of those three states.
            LOG(LS_ERROR) << "Unknown problem connecting to PulseAudio";
        }
        return false;
    }

    // Get the server info.
    pa_operation* op = LATE(pa_context_get_server_info)(pa_context_, paServerInfoCallback, this);
    if (op)
    {
        while (LATE(pa_operation_get_state)(op) == PA_OPERATION_RUNNING)
            LATE(pa_threaded_mainloop_wait)(pa_main_loop_);

        LATE(pa_operation_unref)(op);
    }

    LOG(LS_INFO) << "PulseAudio initialized";
    return true;
}

void AudioOutputPulse::terminatePulseAudio()
{
    // Do nothing if the instance doesn't exist likely pulseSymbolTable.load() fails.
    if (!pa_main_loop_)
        return;

    {
        ScopedPaLock pa_lock(pa_main_loop_);

        if (pa_context_)
        {
            LATE(pa_context_disconnect)(pa_context_);
            LATE(pa_context_unref)(pa_context_);
            pa_context_ = nullptr;
        }
    }

    LATE(pa_threaded_mainloop_stop)(pa_main_loop_);
    LATE(pa_threaded_mainloop_free)(pa_main_loop_);
    pa_main_loop_ = nullptr;

    LOG(LS_INFO) << "PulseAudio terminated";
}

void AudioOutputPulse::threadRun()
{
    LOG(LS_INFO) << "Audio thread started";

    static const struct sched_param kRealTimePrio = {8};
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &kRealTimePrio) != 0)
    {
        LOG(LS_WARNING) << "pthread_setschedparam failed";
    }

    while (true)
    {
        if (!time_event_play_.wait(std::chrono::milliseconds(1000)))
            continue;

        std::scoped_lock lock(mutex_);

        if (quit_)
            return;

        if (start_play_)
        {
            start_play_ = false;

            ScopedPaLock pa_lock(pa_main_loop_);

            // Connect the stream to a sink.
            if (LATE(pa_stream_connect_playback)(play_stream_, nullptr, &play_buffer_attr_,
                static_cast<pa_stream_flags_t>(play_stream_flags_), nullptr, nullptr) != PA_OK)
            {
                LOG(LS_ERROR) << "Failed to connect play stream: "
                              << LATE(pa_context_errno)(pa_context_);
            }

            // Wait for state change.
            while (LATE(pa_stream_get_state)(play_stream_) != PA_STREAM_READY)
                LATE(pa_threaded_mainloop_wait)(pa_main_loop_);

            // We can now handle write callbacks.
            enableWriteCallback();

            playing_ = true;
            play_start_event_.signal();
            continue;
        }

        if (!playing_)
            continue;

        if (playback_buffer_unused_ < playback_buffer_size_)
        {
            size_t write = std::min(
                playback_buffer_size_ - playback_buffer_unused_, temp_buffer_space_);

            ScopedPaLock pa_lock(pa_main_loop_);

            if (LATE(pa_stream_write)(play_stream_, &play_buffer_[playback_buffer_unused_], write,
                                      nullptr, 0, PA_SEEK_RELATIVE) != PA_OK)
            {
                if (write_errors_++ > 10)
                {
                    LOG(LS_ERROR) << "Playout error: " << LATE(pa_context_errno)(pa_context_);
                    write_errors_ = 0;
                }
            }

            playback_buffer_unused_ += write;
            temp_buffer_space_ -= write;
        }

        // Might have been reduced to zero by the above.
        if (temp_buffer_space_ > 0)
        {
            onDataRequest(reinterpret_cast<int16_t*>(play_buffer_.get()), playback_buffer_size_ / 2);

            size_t write = std::min(playback_buffer_size_, temp_buffer_space_);

            ScopedPaLock pa_lock(pa_main_loop_);

            if (LATE(pa_stream_write)(play_stream_, play_buffer_.get(), write,
                                      nullptr, 0, PA_SEEK_RELATIVE) != PA_OK)
            {
                if (write_errors_++ > 10)
                {
                    LOG(LS_ERROR) << "Playout error: " << LATE(pa_context_errno)(pa_context_);
                    write_errors_ = 0;
                }
            }

            playback_buffer_unused_ = write;
        }

        temp_buffer_space_ = 0;

        ScopedPaLock pa_lock(pa_main_loop_);
        enableWriteCallback();
    }
}

} // namespace base
