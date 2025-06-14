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

#include "base/audio/audio_output_pulse.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
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
    Q_DISABLE_COPY(ScopedPaLock)
};

} // namespace

AudioOutputPulse::AudioOutputPulse(const NeedMoreDataCB& need_more_data_cb)
    : AudioOutput(need_more_data_cb)
{
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

    MessageLoop* message_loop = MessageLoop::current();
    if (!message_loop)
    {
        LOG(LS_ERROR) << "No message loop in current thread";
        return false;
    }

    if (message_loop->type() != MessageLoop::Type::ASIO)
    {
        LOG(LS_ERROR) << "Wrong message loop type: " << static_cast<int>(message_loop->type());
        return false;
    }

    timer_ = std::make_unique<asio::high_resolution_timer>(
        message_loop->pumpAsio()->ioContext());
    timer_->expires_after(std::chrono::milliseconds(period_time_));
    timer_->async_wait(
        std::bind(&AudioOutputPulse::onTimerExpired, this, std::placeholders::_1));

    LOG(LS_INFO) << "Audio playout started";
    playing_ = true;
    return true;
}

bool AudioOutputPulse::stop()
{
    if (!playout_initialized_)
        return true;

    if (!play_stream_)
        return false;

    if (timer_)
    {
        timer_->cancel();
        timer_.reset();
    }

    playout_initialized_ = false;
    playing_ = false;

    {
        ScopedPaLock pa_lock(pa_main_loop_);

        // Unset this here so that we don't get a TERMINATED callback.
        LATE(pa_stream_set_state_callback)(play_stream_, nullptr, nullptr);
        LATE(pa_stream_set_write_callback)(play_stream_, nullptr, nullptr);

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
void AudioOutputPulse::paStreamWriteCallback(
    pa_stream* /* stream */, size_t /* buffer_space */, void* self)
{
    static_cast<AudioOutputPulse*>(self)->paStreamWriteCallbackHandler();
}

void AudioOutputPulse::paContextStateCallbackHandler(pa_context* context)
{
    pa_context_state_t state = LATE(pa_context_get_state)(context);
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
    LOG(LS_INFO) << "Native channels: " << static_cast<int>(i->sample_spec.channels);
    LOG(LS_INFO) << "Native format: " << i->sample_spec.format;

    LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
}

void AudioOutputPulse::paStreamStateCallbackHandler(pa_stream* stream)
{
    pa_stream_state_t state = LATE(pa_stream_get_state)(stream);
    switch (state)
    {
        case PA_STREAM_CREATING:
        case PA_STREAM_READY:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_FAILED:
        default:
            LOG(LS_ERROR) << "Stream error: " << LATE(pa_context_errno)(pa_context_);
            LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
            break;
    }
}

void AudioOutputPulse::paStreamWriteCallbackHandler()
{
    LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
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
    pa_sample_spec spec;
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = kSampleRate;
    spec.channels = kChannels;

    ScopedPaLock pa_lock(pa_main_loop_);

    pa_proplist* prop_list = LATE(pa_proplist_new)();
    LATE(pa_proplist_sets)(prop_list, PA_PROP_MEDIA_ROLE, "game");

    // Create a new play stream.
    play_stream_ = LATE(pa_stream_new_with_proplist)
        (pa_context_, "playStream", &spec, nullptr, prop_list);

    LATE(pa_proplist_free)(prop_list);

    if (!play_stream_)
    {
        LOG(LS_ERROR) << "Failed to create play stream: " << LATE(pa_context_errno)(pa_context_);
        return false;
    }

    LATE(pa_stream_set_state_callback)(play_stream_, paStreamStateCallback, this);
    LATE(pa_stream_set_write_callback)(play_stream_, paStreamWriteCallback, this);

    const int kBufferTimeMs = 10;
    const int kBufferSizeMs = 40;
    const int kBytesPerSecond = kSampleRate * kChannels * kBytesPerSample;
    const int kBufferSize = kBytesPerSecond * kBufferSizeMs / 1000LL;

    pa_buffer_attr pa_buffer_attr;
    pa_buffer_attr.fragsize = -1;
    pa_buffer_attr.maxlength = -1;
    pa_buffer_attr.minreq = -1;
    pa_buffer_attr.prebuf = -1;
    pa_buffer_attr.tlength = kBufferSize;

    if (LATE(pa_stream_connect_playback)(play_stream_,
                                         nullptr,
                                         &pa_buffer_attr,
                                         static_cast<pa_stream_flags_t>(0),
                                         nullptr,
                                         nullptr))
    {
        LOG(LS_ERROR) << "Failed to connect play stream: " << LATE(pa_context_errno)(pa_context_);
        return false;
    }

    // Wait for state change.
    while (LATE(pa_stream_get_state)(play_stream_) != PA_STREAM_READY)
        LATE(pa_threaded_mainloop_wait)(pa_main_loop_);

    const struct pa_buffer_attr* buffer = LATE(pa_stream_get_buffer_attr)(play_stream_);
    period_time_ = kBufferTimeMs;
    period_size_ = LATE(pa_usec_to_bytes)(period_time_ * 1000, &spec);
    buffer_size_ = buffer->tlength;
    max_buffer_size_ = buffer->maxlength;

    play_buffer_ = std::make_unique<char[]>(max_buffer_size_);

    // Mark playout side as initialized.
    playout_initialized_ = true;

    LOG(LS_INFO) << "Audio playout initialized";
    return true;
}

void AudioOutputPulse::terminate()
{
    if (!device_initialized_)
        return;

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

void AudioOutputPulse::onTimerExpired(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    if (!error_code)
        writePlayoutData();

    timer_->expires_after(std::chrono::milliseconds(period_time_));
    timer_->async_wait(
        std::bind(&AudioOutputPulse::onTimerExpired, this, std::placeholders::_1));
}

void AudioOutputPulse::writePlayoutData()
{
    while (true)
    {
        int bytes_free;
        {
            ScopedPaLock pa_lock(pa_main_loop_);
            bytes_free = LATE(pa_stream_writable_size)(play_stream_);
        }

        int chunk_count = bytes_free / period_size_;
        if (!chunk_count)
            return;

        int request_size = period_size_;
        if (request_size > max_buffer_size_)
            request_size = max_buffer_size_;

        int samples_count = request_size / kBytesPerSample;

        onDataRequest(reinterpret_cast<int16_t*>(play_buffer_.get()), samples_count);

        {
            ScopedPaLock pa_lock(pa_main_loop_);

            bytes_free = LATE(pa_stream_writable_size)(play_stream_);
            request_size = std::min(request_size, bytes_free);

            if (LATE(pa_stream_write)(play_stream_,
                                      play_buffer_.get(),
                                      request_size,
                                      nullptr,
                                      0,
                                      PA_SEEK_RELATIVE) < 0)
            {
                LOG(LS_ERROR) << "pa_stream_write failed: "
                              << LATE(pa_context_errno)(pa_context_);
            }
        }

        if (chunk_count <= 1)
            break;
    }
}

} // namespace base
