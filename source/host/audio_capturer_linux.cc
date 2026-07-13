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

#include "host/audio_capturer_linux.h"

#include "base/logging.h"
#include "proto/desktop_audio.h"

// Accesses Pulse functions through the late-binding symbol table instead of directly, so the binary
// works on systems without libpulse.
#define LATE(sym) LATESYM_GET(PulseAudioSymbolTable, pulseSymbolTable(), sym)

namespace {

const int kSampleRate = 48000;
const int kChannels = 2;
const int kBytesPerSample = 2;

// The monitor of a digital sink produces exact zeros when nothing plays.
const int kSilenceThreshold = 0;

// Ask the server for 10 ms fragments, matching the granularity the encoder consumes.
const size_t kFragmentSize = kSampleRate * kChannels * kBytesPerSample * 10 / 1000;

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

//--------------------------------------------------------------------------------------------------
AudioCapturerLinux::AudioCapturerLinux(QObject* parent)
    : AudioCapturer(parent),
      silence_detector_(kSilenceThreshold)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioCapturerLinux::~AudioCapturerLinux()
{
    LOG(INFO) << "Dtor";

    if (pa_main_loop_)
    {
        ScopedPaLock pa_lock(pa_main_loop_);
        disconnectStream();
    }

    terminatePulseAudio();
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerLinux::start(const PacketCapturedCallback& callback)
{
    callback_ = callback;
    silence_detector_.reset(kSampleRate, kChannels);

    if (!initPulseAudio())
    {
        LOG(ERROR) << "Failed to initialize PulseAudio";
        terminatePulseAudio();
        return false;
    }

    ScopedPaLock pa_lock(pa_main_loop_);

    if (default_sink_.empty())
    {
        LOG(ERROR) << "No default sink to capture from";
        return false;
    }

    if (!connectStream())
        return false;

    // Wait until the record stream is running (or has failed for good).
    for (;;)
    {
        pa_stream_state_t state = LATE(pa_stream_get_state)(stream_);
        if (state == PA_STREAM_READY)
            break;

        if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED)
        {
            LOG(ERROR) << "Record stream failed:" << LATE(pa_context_errno)(pa_context_);
            return false;
        }

        LATE(pa_threaded_mainloop_wait)(pa_main_loop_);
    }

    // Follow the default sink: server change events re-query the server info, and the stream is
    // moved to the new monitor when the default sink differs from the captured one.
    LATE(pa_context_set_subscribe_callback)(pa_context_, paSubscribeCallback, this);

    pa_operation* op = LATE(pa_context_subscribe)(
        pa_context_, PA_SUBSCRIPTION_MASK_SERVER, nullptr, nullptr);
    if (op)
        LATE(pa_operation_unref)(op);

    LOG(INFO) << "Audio capture started for sink:" << default_sink_;
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
void AudioCapturerLinux::paContextStateCallback(pa_context* context, void* self)
{
    static_cast<AudioCapturerLinux*>(self)->paContextStateCallbackHandler(context);
}

//--------------------------------------------------------------------------------------------------
// static
void AudioCapturerLinux::paServerInfoCallback(pa_context*, const pa_server_info* info, void* self)
{
    static_cast<AudioCapturerLinux*>(self)->paServerInfoCallbackHandler(info);
}

//--------------------------------------------------------------------------------------------------
// static
void AudioCapturerLinux::paSubscribeCallback(pa_context* context,
                                             pa_subscription_event_type_t /* type */,
                                             uint32_t /* index */, void* self)
{
    // A server-level change (the mask limits the subscription to those): re-query the server info to
    // see whether the default sink moved. The handler runs on this same mainloop thread.
    pa_operation* op = LATE(pa_context_get_server_info)(context, paServerInfoCallback, self);
    if (op)
        LATE(pa_operation_unref)(op);
}

//--------------------------------------------------------------------------------------------------
// static
void AudioCapturerLinux::paStreamStateCallback(pa_stream* stream, void* self)
{
    static_cast<AudioCapturerLinux*>(self)->paStreamStateCallbackHandler(stream);
}

//--------------------------------------------------------------------------------------------------
// static
void AudioCapturerLinux::paStreamReadCallback(pa_stream* stream, size_t /* bytes */, void* self)
{
    static_cast<AudioCapturerLinux*>(self)->paStreamReadCallbackHandler(stream);
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::paContextStateCallbackHandler(pa_context* context)
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
        case PA_CONTEXT_READY:
            pa_state_changed_ = true;
            LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::paServerInfoCallbackHandler(const pa_server_info* info)
{
    std::string default_sink = (info && info->default_sink_name) ? info->default_sink_name : "";

    if (default_sink != default_sink_)
    {
        default_sink_ = default_sink;

        // The default sink changed while capturing: move the capture to the new monitor. Runs on
        // the mainloop thread, so the stream is reconnected right here without extra locking.
        if (stream_)
        {
            LOG(INFO) << "Default sink changed, capturing from:" << default_sink_;

            disconnectStream();
            if (!default_sink_.empty())
                connectStream();
        }
    }

    LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::paStreamStateCallbackHandler(pa_stream* stream)
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
            LOG(ERROR) << "Stream error:" << LATE(pa_context_errno)(pa_context_);
            break;
    }

    LATE(pa_threaded_mainloop_signal)(pa_main_loop_, 0);
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::paStreamReadCallbackHandler(pa_stream* stream)
{
    // Drain everything the server buffered. peek() may report a hole (null data with a size); it is
    // skipped by the same drop() that releases normal chunks.
    while (LATE(pa_stream_readable_size)(stream) > 0)
    {
        const void* data = nullptr;
        size_t bytes = 0;

        if (LATE(pa_stream_peek)(stream, &data, &bytes) < 0)
        {
            LOG(ERROR) << "pa_stream_peek failed:" << LATE(pa_context_errno)(pa_context_);
            return;
        }

        if (!bytes)
            break;

        if (data)
            processChunk(data, bytes);

        LATE(pa_stream_drop)(stream);
    }
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerLinux::initPulseAudio()
{
    if (!pulseSymbolTable()->load())
    {
        // Most likely the Pulse library and sound server are not installed on this system.
        LOG(ERROR) << "Failed to load symbol table";
        return false;
    }

    if (pa_main_loop_)
    {
        LOG(ERROR) << "PA mainloop has already existed";
        return false;
    }

    pa_main_loop_ = LATE(pa_threaded_mainloop_new)();
    if (!pa_main_loop_)
    {
        LOG(ERROR) << "Could not create mainloop";
        return false;
    }

    int ret = LATE(pa_threaded_mainloop_start)(pa_main_loop_);
    if (ret != PA_OK)
    {
        LOG(ERROR) << "Failed to start main loop:" << ret;
        return false;
    }

    ScopedPaLock pa_lock(pa_main_loop_);

    pa_main_loop_api_ = LATE(pa_threaded_mainloop_get_api)(pa_main_loop_);
    if (!pa_main_loop_api_)
    {
        LOG(ERROR) << "Could not create mainloop API";
        return false;
    }

    pa_context_ = LATE(pa_context_new)(pa_main_loop_api_, "Aspia Host");
    if (!pa_context_)
    {
        LOG(ERROR) << "Could not create context";
        return false;
    }

    LATE(pa_context_set_state_callback)(pa_context_, paContextStateCallback, this);

    pa_state_changed_ = false;
    ret = LATE(pa_context_connect)(pa_context_, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);
    if (ret != PA_OK)
    {
        LOG(ERROR) << "Failed to connect context:" << ret;
        return false;
    }

    while (!pa_state_changed_)
        LATE(pa_threaded_mainloop_wait)(pa_main_loop_);

    pa_context_state_t state = LATE(pa_context_get_state)(pa_context_);
    if (state != PA_CONTEXT_READY)
    {
        LOG(ERROR) << "Failed to connect to PulseAudio sound server, state:" << state;
        return false;
    }

    // Fetch the server info for the default sink name whose monitor will be captured.
    pa_operation* op = LATE(pa_context_get_server_info)(pa_context_, paServerInfoCallback, this);
    if (op)
    {
        while (LATE(pa_operation_get_state)(op) == PA_OPERATION_RUNNING)
            LATE(pa_threaded_mainloop_wait)(pa_main_loop_);

        LATE(pa_operation_unref)(op);
    }

    LOG(INFO) << "PulseAudio initialized";
    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::terminatePulseAudio()
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

    LOG(INFO) << "PulseAudio terminated";
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerLinux::connectStream()
{
    pa_sample_spec spec;
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = kSampleRate;
    spec.channels = kChannels;

    stream_ = LATE(pa_stream_new_with_proplist)(pa_context_, "captureStream", &spec, nullptr,
                                                nullptr);
    if (!stream_)
    {
        LOG(ERROR) << "Failed to create record stream:" << LATE(pa_context_errno)(pa_context_);
        return false;
    }

    LATE(pa_stream_set_state_callback)(stream_, paStreamStateCallback, this);
    LATE(pa_stream_set_read_callback)(stream_, paStreamReadCallback, this);

    pa_buffer_attr attr;
    attr.fragsize = kFragmentSize;
    attr.maxlength = static_cast<uint32_t>(-1);
    attr.minreq = static_cast<uint32_t>(-1);
    attr.prebuf = static_cast<uint32_t>(-1);
    attr.tlength = static_cast<uint32_t>(-1);

    // The monitor source of a sink carries what is played through it; PulseAudio names it after the
    // sink itself.
    const std::string monitor = default_sink_ + ".monitor";

    if (LATE(pa_stream_connect_record)(stream_, monitor.c_str(), &attr, PA_STREAM_ADJUST_LATENCY))
    {
        LOG(ERROR) << "Failed to connect record stream:" << LATE(pa_context_errno)(pa_context_);
        disconnectStream();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::disconnectStream()
{
    if (!stream_)
        return;

    // Unset the callbacks here so that we don't get a TERMINATED callback or read stale data.
    LATE(pa_stream_set_state_callback)(stream_, nullptr, nullptr);
    LATE(pa_stream_set_read_callback)(stream_, nullptr, nullptr);

    if (LATE(pa_stream_get_state)(stream_) != PA_STREAM_UNCONNECTED)
        LATE(pa_stream_disconnect)(stream_);

    LATE(pa_stream_unref)(stream_);
    stream_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerLinux::processChunk(const void* data, size_t bytes)
{
    const size_t frames = bytes / (kBytesPerSample * kChannels);

    if (silence_detector_.isSilence(static_cast<const qint16*>(data), frames))
        return;

    std::unique_ptr<proto::audio::Packet> packet(new proto::audio::Packet());
    packet->add_data(data, bytes);
    packet->set_encoding(proto::audio::ENCODING_RAW);
    packet->set_sampling_rate(proto::audio::Packet::SAMPLING_RATE_48000);
    packet->set_bytes_per_sample(proto::audio::Packet::BYTES_PER_SAMPLE_2);
    packet->set_channels(proto::audio::Packet::CHANNELS_STEREO);

    callback_(std::move(packet));
}
