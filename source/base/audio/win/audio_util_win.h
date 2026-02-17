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

#ifndef BASE_AUDIO_WIN_AUDIO_UTIL_WIN_H
#define BASE_AUDIO_WIN_AUDIO_UTIL_WIN_H

#include <QtGlobal>

#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace base {

Microsoft::WRL::ComPtr<IMMDeviceEnumerator> createDeviceEnumerator(bool allow_reinitialize);
Microsoft::WRL::ComPtr<IMMDevice> createDevice();
Microsoft::WRL::ComPtr<IAudioClient> createClient(IMMDevice* audio_device);
Microsoft::WRL::ComPtr<IAudioRenderClient> createRenderClient(IAudioClient* client);
Microsoft::WRL::ComPtr<IAudioSessionControl> createAudioSessionControl(IAudioClient* client);

bool sharedModeInitialize(IAudioClient* client,
                          const WAVEFORMATEXTENSIBLE* format,
                          HANDLE event_handle,
                          REFERENCE_TIME buffer_duration,
                          bool auto_convert_pcm,
                          quint32* endpoint_buffer_size);

bool isFormatSupported(IAudioClient* client,
                       AUDCLNT_SHAREMODE share_mode,
                       const WAVEFORMATEXTENSIBLE* format);

bool fillRenderEndpointBufferWithSilence(IAudioClient* client, IAudioRenderClient* render_client);

bool isMMCSSSupported();
bool isDeviceActive(IMMDevice* device);

} // namespace base

#endif // BASE_AUDIO_WIN_AUDIO_UTIL_WIN_H
