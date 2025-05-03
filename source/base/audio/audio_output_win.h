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

#ifndef BASE_AUDIO_AUDIO_OUTPUT_WIN_H
#define BASE_AUDIO_AUDIO_OUTPUT_WIN_H

#include "base/audio/audio_output.h"
#include "base/win/scoped_object.h"

#include <atomic>
#include <memory>

#include <Audioclient.h>
#include <audiopolicy.h>
#include <wrl/client.h>

namespace base {

class SimpleThread;

class AudioOutputWin final
    : public AudioOutput,
      public IAudioSessionEvents
{
public:
    explicit AudioOutputWin(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputWin() final;

    // AudioOutput implementation.
    bool start() final;
    bool stop() final;

private:
    void threadRun();
    bool init();
    bool handleDataRequest();
    bool handleRestartEvent();
    void stopThread();
    void releaseCOMObjects();

    // IUnknown (required by IAudioSessionEvents and IMMNotificationClient).
    ULONG __stdcall AddRef() final;
    ULONG __stdcall Release() final;
    HRESULT __stdcall QueryInterface(REFIID iid, void** object) final;

    // IAudioSessionEvents implementation.
    // These methods are called on separate threads owned by the session manager. More than one
    // thread can be involved depending on the type of callback and audio session.
    HRESULT __stdcall OnStateChanged(AudioSessionState new_state) final;
    HRESULT __stdcall OnSessionDisconnected(
        AudioSessionDisconnectReason disconnect_reason) final;
    HRESULT __stdcall OnDisplayNameChanged(LPCWSTR new_display_name,
                                           LPCGUID event_context) final;
    HRESULT __stdcall OnIconPathChanged(LPCWSTR new_icon_path,
                                        LPCGUID event_context) final;
    HRESULT __stdcall OnSimpleVolumeChanged(float new_simple_volume,
                                            BOOL new_mute,
                                            LPCGUID event_context) final;
    HRESULT __stdcall OnChannelVolumeChanged(DWORD channel_count,
                                             float new_channel_volumes[],
                                             DWORD changed_channel,
                                             LPCGUID event_context) final;
    HRESULT __stdcall OnGroupingParamChanged(LPCGUID new_grouping_param,
                                             LPCGUID event_context) final;

    bool is_initialized_ = false;
    bool is_active_ = false;
    std::atomic_bool is_restarting_ = false;

    Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
    Microsoft::WRL::ComPtr<IAudioRenderClient> audio_render_client_;
    Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control_;

    std::unique_ptr<SimpleThread> audio_thread_;

    ScopedHandle audio_samples_event_;
    ScopedHandle stop_event_;
    ScopedHandle restart_event_;

    uint32_t endpoint_buffer_size_frames_ = 0;
    quint64 num_frames_written_ = 0;

    // Used by the IAudioSessionEvents implementations.
    // Currently only utilized for debugging purposes.
    LONG ref_count_ = 1;

    DISALLOW_COPY_AND_ASSIGN(AudioOutputWin);
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_OUTPUT_WIN_H
