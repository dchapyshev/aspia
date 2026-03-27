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

#include "base/audio/audio_output_win.h"

#include <comdef.h>

#include "base/logging.h"
#include "base/audio/win/audio_util_win.h"
#include "base/audio/win/scoped_mmcss_registration.h"
#include "base/win/scoped_com_initializer.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
const char* sessionStateToString(AudioSessionState state)
{
    switch (state)
    {
        case AudioSessionStateActive:
            return "Active";
        case AudioSessionStateInactive:
            return "Inactive";
        case AudioSessionStateExpired:
            return "Expired";
        default:
            return "Invalid";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
AudioOutputWin::AudioOutputWin(const NeedMoreDataCB& need_more_data_cb)
    : AudioOutput(need_more_data_cb)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioOutputWin::~AudioOutputWin()
{
    LOG(INFO) << "Dtor";
    stop();
}

//--------------------------------------------------------------------------------------------------
bool AudioOutputWin::start()
{
    if (started_)
    {
        LOG(WARNING) << "Already started";
        return true;
    }

    // Create the event which the audio engine will signal each time a buffer becomes ready to be
    // processed by the client.
    audio_samples_event_.reset(CreateEventW(nullptr, false, false, nullptr));
    DCHECK(audio_samples_event_.isValid());

    // Event to be set in Stop() when rendering/capturing shall stop.
    stop_event_.reset(CreateEventW(nullptr, false, false, nullptr));
    DCHECK(stop_event_.isValid());

    // Event to be set when it has been detected that an active device has been invalidated or the
    // stream format has changed.
    restart_event_.reset(CreateEventW(nullptr, false, false, nullptr));
    DCHECK(restart_event_.isValid());

    if (!startStreaming())
    {
        LOG(ERROR) << "Unable to start streaming";
        return false;
    }

    audio_thread_ = std::make_unique<AudioThread>(this);
    audio_thread_->start();

    started_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool AudioOutputWin::stop()
{
    is_restarting_ = false;
    started_ = false;

    if (audio_thread_ && audio_thread_->isRunning())
    {
        SetEvent(stop_event_);
        audio_thread_->wait();
        audio_thread_.reset();
    }

    stopStreaming();
    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioOutputWin::threadRun()
{
    if (!isMMCSSSupported())
    {
        LOG(ERROR) << "MMCSS is not supported";
        return;
    }

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    ScopedMMCSSRegistration mmcss_registration(L"Pro Audio");
    ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);

    DCHECK(mmcss_registration.isSucceeded());
    DCHECK(com_initializer.isSucceeded());
    DCHECK(stop_event_.isValid());
    DCHECK(audio_samples_event_.isValid());

    bool streaming = true;
    bool error = false;
    HANDLE wait_array[] = { stop_event_.get(), restart_event_.get(), audio_samples_event_.get() };

    // Keep streaming audio until the stop event or the stream-switch event
    // is signaled. An error event can also break the main thread loop.
    while (streaming && !error)
    {
        // Wait for a close-down event, stream-switch event or a new render event.
        DWORD wait_result = WaitForMultipleObjects(
            static_cast<DWORD>(std::size(wait_array)), wait_array, false, INFINITE);
        switch (wait_result)
        {
            case WAIT_OBJECT_0 + 0:
                // |stop_event_| has been set.
                streaming = false;
                break;
            case WAIT_OBJECT_0 + 1:
                // |restart_event_| has been set.
                error = !handleRestartEvent();
                break;
            case WAIT_OBJECT_0 + 2:
                // |audio_samples_event_| has been set.
                error = !handleDataRequest();
                break;
            default:
                error = true;
                break;
        }
    }

    if (streaming && error)
    {
        LOG(ERROR) << "WASAPI streaming failed";
        is_restarting_ = false;
        started_ = false;

        // Stop audio streaming since something has gone wrong in our main thread loop.
        // A stop() call is still required to join the thread.
        if (audio_client_)
        {
            _com_error error = audio_client_->Stop();
            if (FAILED(error.Error()))
                LOG(ERROR) << "IAudioClient::Stop failed:" << error;
        }
    }
}

//--------------------------------------------------------------------------------------------------
bool AudioOutputWin::handleDataRequest()
{
    // Get the padding value which indicates the amount of valid unread data that the endpoint
    // buffer currently contains.
    UINT32 num_unread_frames = 0;
    _com_error error = audio_client_->GetCurrentPadding(&num_unread_frames);
    if (error.Error() == AUDCLNT_E_DEVICE_INVALIDATED)
    {
        // Trigger a restart if not already in progress. This serves as a safety net in case
        // OnSessionDisconnected was not called or did not initiate a restart.
        if (!is_restarting_)
        {
            is_restarting_ = true;
            SetEvent(restart_event_);
        }
        return true;
    }

    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IAudioClient::GetCurrentPadding failed:" << error;
        return false;
    }

    // Contains how much new data we can write to the buffer without the risk of overwriting
    // previously written data that the audio engine has not yet read from the buffer. I.e., it is
    // the maximum buffer size we can request when calling IAudioRenderClient::GetBuffer().
    UINT32 num_requested_frames = endpoint_buffer_size_frames_ - num_unread_frames;
    if (num_requested_frames == 0)
        return true;

    // Request all available space in the rendering endpoint buffer into which the client can later
    // write an audio packet.
    quint8* audio_data;
    error = audio_render_client_->GetBuffer(num_requested_frames, &audio_data);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IAudioRenderClient::GetBuffer failed:" << error;
        return false;
    }

    // Get audio data and write it to the allocated buffer in |audio_data|. The playout latency is
    // not updated for each callback.
    onDataRequest(reinterpret_cast<qint16*>(audio_data), num_requested_frames * kChannels);

    // Release the buffer space acquired in IAudioRenderClient::GetBuffer.
    error = audio_render_client_->ReleaseBuffer(num_requested_frames, 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IAudioRenderClient::ReleaseBuffer failed:" << error;
        return false;
    }

    num_frames_written_ += num_requested_frames;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool AudioOutputWin::handleRestartEvent()
{
    DCHECK(audio_thread_);
    DCHECK(is_restarting_);

    is_restarting_ = false;

    stopStreaming();
    return startStreaming();
}

//--------------------------------------------------------------------------------------------------
void AudioOutputWin::stopStreaming()
{
    if (audio_client_)
    {
        // Stop audio streaming.
        _com_error error = audio_client_->Stop();
        if (FAILED(error.Error()))
            LOG(ERROR) << "IAudioClient::Stop failed:" << error;

        error = audio_client_->Reset();
        if (FAILED(error.Error()))
            LOG(ERROR) << "IAudioClient::Reset failed:" << error;
    }

    if (audio_session_control_.Get())
    {
        audio_session_control_->UnregisterAudioSessionNotification(this);
        audio_session_control_.Reset();
    }

    if (audio_render_client_.Get())
        audio_render_client_.Reset();

    if (audio_client_)
        audio_client_.Reset();
}

//--------------------------------------------------------------------------------------------------
bool AudioOutputWin::startStreaming()
{
    Microsoft::WRL::ComPtr<IMMDevice> device(createDevice());
    if (!device.Get())
    {
        LOG(ERROR) << "createDevice failed";
        return false;
    }

    Microsoft::WRL::ComPtr<IAudioClient> audio_client = createClient(device.Get());
    if (!audio_client.Get())
    {
        LOG(ERROR) << "createClient failed";
        return false;
    }

    // Define the output WAVEFORMATEXTENSIBLE format in |format_|.
    WAVEFORMATEXTENSIBLE format_extensible;
    memset(&format_extensible, 0, sizeof(format_extensible));

    WAVEFORMATEX& format = format_extensible.Format;
    format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    format.nChannels       = kChannels;
    format.nSamplesPerSec  = kSampleRate;
    format.wBitsPerSample  = kBitsPerSample;
    format.nBlockAlign     = (format.wBitsPerSample / 8) * format.nChannels;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    // Add the parts which are unique for the WAVE_FORMAT_EXTENSIBLE structure.
    format_extensible.Samples.wValidBitsPerSample = kBitsPerSample;
    format_extensible.dwChannelMask               = KSAUDIO_SPEAKER_STEREO;
    format_extensible.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;

    if (!isFormatSupported(audio_client.Get(), AUDCLNT_SHAREMODE_SHARED, &format_extensible))
    {
        LOG(ERROR) << "Format not supported";
        return false;
    }

    // Initialize the audio stream between the client and the device in shared mode using
    // event-driven buffer handling. Also, using 0 as requested buffer size results in a default
    // (minimum) endpoint buffer size.
    const REFERENCE_TIME requested_buffer_size = 0;
    if (!sharedModeInitialize(audio_client.Get(), &format_extensible, audio_samples_event_,
                              requested_buffer_size, true, &endpoint_buffer_size_frames_))
    {
        LOG(ERROR) << "sharedModeInitialize failed";
        return false;
    }

    // Create an IAudioRenderClient for an initialized IAudioClient. The IAudioRenderClient
    // interface enables us to write output data to a rendering endpoint buffer.
    Microsoft::WRL::ComPtr<IAudioRenderClient> audio_render_client =
        createRenderClient(audio_client.Get());
    if (!audio_render_client.Get())
    {
        LOG(ERROR) << "createRenderClient failed";
        return false;
    }

    // Create an AudioSessionControl interface given the initialized client. The IAudioControl
    // interface enables a client to configure the control parameters for an audio session and to
    // monitor events in the session.
    Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control =
        createAudioSessionControl(audio_client.Get());
    if (!audio_session_control.Get())
    {
        LOG(ERROR) << "createAudioSessionControl failed";
        return false;
    }

    // The Sndvol program displays volume and mute controls for sessions that are in the active and
    // inactive states.
    AudioSessionState state;
    if (FAILED(audio_session_control->GetState(&state)))
        return false;

    LOG(INFO) << "Audio session state:" << sessionStateToString(state);

    // Register the client to receive notifications of session events, including changes in the
    // stream state.
    if (FAILED(audio_session_control->RegisterAudioSessionNotification(this)))
    {
        LOG(ERROR) << "IAudioSessionControl::RegisterAudioSessionNotification failed";
        return false;
    }

    if (!fillRenderEndpointBufferWithSilence(audio_client.Get(), audio_render_client.Get()))
        LOG(ERROR) << "Failed to prepare output endpoint with silence";

    _com_error error = audio_client->Start();
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IAudioClient::Start failed:" << error;
        audio_session_control->UnregisterAudioSessionNotification(this);
        return false;
    }

    // Store valid COM interfaces.
    audio_client_ = audio_client;
    audio_render_client_ = audio_render_client;
    audio_session_control_ = audio_session_control;
    num_frames_written_ = endpoint_buffer_size_frames_;
    return true;
}

//--------------------------------------------------------------------------------------------------
ULONG AudioOutputWin::AddRef()
{
    ULONG new_ref = InterlockedIncrement(&ref_count_);
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
ULONG AudioOutputWin::Release()
{
    ULONG new_ref = InterlockedDecrement(&ref_count_);
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::QueryInterface(REFIID iid, void** object)
{
    if (object == nullptr)
        return E_POINTER;

    if (iid == IID_IUnknown || iid == __uuidof(IAudioSessionEvents))
    {
        *object = static_cast<IAudioSessionEvents*>(this);
        return S_OK;
    };

    *object = nullptr;
    return E_NOINTERFACE;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::OnStateChanged(AudioSessionState /* new_state */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::OnSessionDisconnected(AudioSessionDisconnectReason disconnect_reason)
{
    LOG(INFO) << "Session disconnected, reason:" << static_cast<int>(disconnect_reason);

    if (is_restarting_)
    {
        LOG(WARNING) << "Ignoring since restart is already active";
        return S_OK;
    }

    // Restart for any disconnect reason. If the device is permanently gone, startStreaming() will
    // fail during restart and the thread will exit cleanly through the error path.
    is_restarting_ = true;
    SetEvent(restart_event_);
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::OnDisplayNameChanged(
    LPCWSTR /* new_display_name */, LPCGUID /* event_context */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::OnIconPathChanged(LPCWSTR /* new_icon_path */, LPCGUID /* event_context */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::OnSimpleVolumeChanged(
    float /* new_simple_volume */, BOOL /* new_mute */, LPCGUID /* event_context */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::OnChannelVolumeChanged(
    DWORD /* channel_count */, float /* new_channel_volumes */[], DWORD /* changed_channel */,
    LPCGUID /* event_context */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT AudioOutputWin::OnGroupingParamChanged(
    LPCGUID /* new_grouping_param */, LPCGUID /* event_context */)
{
    return S_OK;
}

} // namespace base
