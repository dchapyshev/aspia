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

#include "base/audio/win/audio_util_win.h"

#include "base/logging.h"
#include "base/win/scoped_co_mem.h"

namespace base {

//--------------------------------------------------------------------------------------------------
Microsoft::WRL::ComPtr<IMMDeviceEnumerator> createDeviceEnumerator(bool allow_reinitialize)
{
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> device_enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&device_enumerator));
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "CoCreateInstance failed: " << SystemError(hr).toString();
    }

    if (hr == CO_E_NOTINITIALIZED && allow_reinitialize)
    {
        LOG(LS_ERROR) << "CoCreateInstance failed with CO_E_NOTINITIALIZED";

        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr))
        {
            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&device_enumerator));
            if (FAILED(hr))
            {
                LOG(LS_ERROR) << "CoCreateInstance failed: " << SystemError(hr).toString();
            }
        }
    }

    return device_enumerator;
}

//--------------------------------------------------------------------------------------------------
// Retrieve an audio device specified by |device_id| or a default device
// specified by data-flow direction and role if |device_id| is default.
Microsoft::WRL::ComPtr<IMMDevice> createDevice()
{
    Microsoft::WRL::ComPtr<IMMDevice> audio_endpoint_device;

    // Create the IMMDeviceEnumerator interface.
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> device_enum(createDeviceEnumerator(true));
    if (!device_enum.Get())
    {
        LOG(LS_ERROR) << "createDeviceEnumerator failed";
        return audio_endpoint_device;
    }

    // Get the default audio endpoint for the specified data-flow direction and // role. Note that,
    // if only a single rendering or capture device is available, the system always assigns all
    // three rendering or capture roles to that device. If the method fails to find a rendering or
    // capture device for the specified role, this means that no rendering or capture device is
    // available at all. If no device is available, the method sets the output pointer to NULL and
    // returns ERROR_NOT_FOUND.
    HRESULT hr = device_enum->GetDefaultAudioEndpoint(
        eRender, eConsole, audio_endpoint_device.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: "
                      << SystemError(hr).toString();
    }

    // Verify that the audio endpoint device is active, i.e., that the audio
    // adapter that connects to the endpoint device is present and enabled.
    if (SUCCEEDED(hr) && !audio_endpoint_device.Get() &&
        !isDeviceActive(audio_endpoint_device.Get()))
    {
        LOG(LS_ERROR) << "Selected endpoint device is not active";
        audio_endpoint_device.Reset();
    }

    return audio_endpoint_device;
}

//--------------------------------------------------------------------------------------------------
// Creates and activates an IAudioClient COM object given the selected endpoint device.
Microsoft::WRL::ComPtr<IAudioClient> createClient(IMMDevice* audio_device)
{
    if (!audio_device)
        return Microsoft::WRL::ComPtr<IAudioClient>();

    Microsoft::WRL::ComPtr<IAudioClient> audio_client;
    HRESULT hr = audio_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                        nullptr, &audio_client);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IMMDevice::Activate(IAudioClient) failed: "
                      << SystemError(hr).toString();
    }
    return audio_client;
}

//--------------------------------------------------------------------------------------------------
Microsoft::WRL::ComPtr<IAudioRenderClient> createRenderClient(IAudioClient* client)
{
    DCHECK(client);
    // Get access to the IAudioRenderClient interface. This interface
    // enables us to write output data to a rendering endpoint buffer.
    Microsoft::WRL::ComPtr<IAudioRenderClient> audio_render_client;
    HRESULT hr = client->GetService(IID_PPV_ARGS(&audio_render_client));
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioClient::GetService(IID_IAudioRenderClient) failed: "
                      << SystemError(hr).toString();
        return Microsoft::WRL::ComPtr<IAudioRenderClient>();
    }
    return audio_render_client;
}

//--------------------------------------------------------------------------------------------------
Microsoft::WRL::ComPtr<IAudioSessionControl> createAudioSessionControl(IAudioClient* client)
{
    DCHECK(client);
    Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control;
    HRESULT hr = client->GetService(IID_PPV_ARGS(&audio_session_control));
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioClient::GetService(IID_IAudioControl) failed: "
                      << SystemError(hr).toString();
        return Microsoft::WRL::ComPtr<IAudioSessionControl>();
    }
    return audio_session_control;
}

//--------------------------------------------------------------------------------------------------
bool sharedModeInitialize(IAudioClient* client,
                          const WAVEFORMATEXTENSIBLE* format,
                          HANDLE event_handle,
                          REFERENCE_TIME buffer_duration,
                          bool auto_convert_pcm,
                          uint32_t* endpoint_buffer_size)
{
    DCHECK(client);
    DCHECK_GE(buffer_duration, 0);

    if (buffer_duration != 0)
    {
        LOG(LS_INFO) << "Non-default buffer size is used";
    }
    if (auto_convert_pcm)
    {
        LOG(LS_INFO) << "Sample rate converter can be utilized";
    }
    // The AUDCLNT_STREAMFLAGS_NOPERSIST flag disables persistence of the volume and mute settings
    // for a session that contains rendering streams. By default, the volume level and muting state
    // for a rendering session are persistent across system restarts. The volume level and muting
    // state for a capture session are never persistent.
    DWORD stream_flags = AUDCLNT_STREAMFLAGS_NOPERSIST;

    // Enable event-driven streaming if a valid event handle is provided. After the stream starts,
    // the audio engine will signal the event handle to notify the client each time a buffer
    // becomes ready to process. Event-driven buffering is supported for both rendering and capturing.
    // Both shared-mode and exclusive-mode streams can use event-driven buffering.
    bool use_event = (event_handle != nullptr && event_handle != INVALID_HANDLE_VALUE);
    if (use_event)
    {
        stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        LOG(LS_INFO) << "The stream is initialized to be event driven";
    }

    // Check if sample-rate conversion is requested.
    if (auto_convert_pcm)
    {
        // Add channel matrixer (not utilized here) and rate converter to convert from our (the
        // client's) format to the audio engine mix format. Currently only supported for testing,
        // i.e., not possible to enable using public APIs.
        LOG(LS_INFO) << "The stream is initialized to support rate conversion";
        stream_flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
        stream_flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    }

    // Initialize the shared mode client for minimal delay if |buffer_duration| is 0 or possibly a
    // higher delay (more robust) if |buffer_duration| is larger than 0. The actual size is given
    // by IAudioClient::GetBufferSize().
    HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags, buffer_duration, 0,
                                    reinterpret_cast<const WAVEFORMATEX*>(format), nullptr);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioClient::Initialize failed: " << SystemError(hr).toString();
        return false;
    }

    // If a stream is initialized to be event driven and in shared mode, the associated application
    // must also obtain a handle by making a call to IAudioClient::SetEventHandle.
    if (use_event)
    {
        hr = client->SetEventHandle(event_handle);
        if (FAILED(hr))
        {
            LOG(LS_ERROR) << "IAudioClient::SetEventHandle failed: " << SystemError(hr).toString();
            return false;
        }
    }

    UINT32 buffer_size_in_frames = 0;
    // Retrieves the size (maximum capacity) of the endpoint buffer. The size is expressed as the
    // number of audio frames the buffer can hold. For rendering clients, the buffer length
    // determines the maximum amount of rendering data that the application can write to the
    // endpoint buffer during a single processing pass. For capture clients, the buffer length
    // determines the maximum amount of capture data that the audio engine can read from the
    // endpoint buffer during a single processing pass.
    hr = client->GetBufferSize(&buffer_size_in_frames);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioClient::GetBufferSize failed: " << SystemError(hr).toString();
        return false;
    }

    *endpoint_buffer_size = buffer_size_in_frames;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool isFormatSupported(IAudioClient* client,
                       AUDCLNT_SHAREMODE share_mode,
                       const WAVEFORMATEXTENSIBLE* format)
{
    DCHECK(client);

    ScopedCoMem<WAVEFORMATEX> closest_match;
    // This method provides a way for a client to determine, before calling
    // IAudioClient::Initialize, whether the audio engine supports a particular
    // stream format or not. In shared mode, the audio engine always supports
    // the mix format (see GetSharedModeMixFormat).
    // TODO(henrika): verify support for exclusive mode as well?
    HRESULT hr = client->IsFormatSupported(share_mode,
                                           reinterpret_cast<const WAVEFORMATEX*>(format),
                                           &closest_match);
    if ((hr == S_OK) && (closest_match == nullptr))
    {
        LOG(LS_INFO) << "Audio device does not support specified format";
    }
    else if ((hr == S_FALSE) && (closest_match != nullptr))
    {
        // Call succeeded with a closest match to the specified format. This log can
        // only be triggered for shared mode.
        LOG(LS_ERROR) << "Exact format is not supported, but a closest match exists";
    }
    else if ((hr == AUDCLNT_E_UNSUPPORTED_FORMAT) && (closest_match == nullptr))
    {
        // The audio engine does not support the caller-specified format or any
        // similar format.
        LOG(LS_INFO) << "Audio device does not support specified format";
    }
    else
    {
        LOG(LS_ERROR) << "IAudioClient::IsFormatSupported failed: "
                      << SystemError(hr).toString();
    }

    return (hr == S_OK);
}

//--------------------------------------------------------------------------------------------------
bool fillRenderEndpointBufferWithSilence(IAudioClient* client, IAudioRenderClient* render_client)
{
    DCHECK(client);
    DCHECK(render_client);

    UINT32 endpoint_buffer_size = 0;
    HRESULT hr = client->GetBufferSize(&endpoint_buffer_size);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioClient::GetBufferSize failed: " << SystemError(hr).toString();
        return false;
    }

    UINT32 num_queued_frames = 0;
    // Get number of audio frames that are queued up to play in the endpoint
    // buffer.
    hr = client->GetCurrentPadding(&num_queued_frames);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioClient::GetCurrentPadding failed: "
                      << SystemError(hr).toString();
        return false;
    }
    LOG(LS_INFO) << "num_queued_frames: " << num_queued_frames;

    BYTE* data = nullptr;
    int num_frames_to_fill = endpoint_buffer_size - num_queued_frames;
    LOG(LS_INFO) << "num_frames_to_fill: " << num_frames_to_fill;

    hr = render_client->GetBuffer(num_frames_to_fill, &data);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioRenderClient::GetBuffer failed: " << SystemError(hr).toString();
        return false;
    }

    // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
    // explicitly write silence data to the rendering buffer.
    hr = render_client->ReleaseBuffer(num_frames_to_fill, AUDCLNT_BUFFERFLAGS_SILENT);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "IAudioRenderClient::ReleaseBuffer failed: "
                      << SystemError(hr).toString();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool isMMCSSSupported()
{
    static const wchar_t* const kAvrtDLL = L"%WINDIR%\\system32\\Avrt.dll";
    wchar_t path[MAX_PATH] = { 0 };

    ExpandEnvironmentStringsW(kAvrtDLL, path, static_cast<DWORD>(std::size(path)));
    return (LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) != nullptr);
}

//--------------------------------------------------------------------------------------------------
bool isDeviceActive(IMMDevice* device)
{
    DWORD state = DEVICE_STATE_DISABLED;
    return SUCCEEDED(device->GetState(&state)) && (state & DEVICE_STATE_ACTIVE);
}

} // namespace base
