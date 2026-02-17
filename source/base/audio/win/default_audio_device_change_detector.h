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

#ifndef BASE_AUDIO_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H
#define BASE_AUDIO_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H

#include <mutex>

#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace base {

// An IMMNotificationClient implementation to detect the change of the default audio output device
// on the system. It registers itself into the input IMMDeviceEnumerator in constructor and
// unregisters in destructor.
// This class does not use the default ref-counting memory management method provided by IUnknown:
// calling DefaultAudioDeviceChangeDetector::Release() won't delete the object.
class DefaultAudioDeviceChangeDetector final : public IMMNotificationClient
{
public:
    explicit DefaultAudioDeviceChangeDetector(
        const Microsoft::WRL::ComPtr<IMMDeviceEnumerator>& enumerator);
    ~DefaultAudioDeviceChangeDetector();

    bool getAndReset();

private:
    // IMMNotificationClient implementation.
    HRESULT __stdcall OnDefaultDeviceChanged(EDataFlow flow,
                                             ERole role,
                                             LPCWSTR pwstrDefaultDevice) final;
    HRESULT __stdcall QueryInterface(REFIID iid, void** object) final;

    // No-ops overrides.
    HRESULT __stdcall OnDeviceAdded(LPCWSTR pwstrDeviceId) final;
    HRESULT __stdcall OnDeviceRemoved(LPCWSTR pwstrDeviceId) final;
    HRESULT __stdcall OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) final;
    HRESULT __stdcall OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) final;
    ULONG __stdcall AddRef() final;
    ULONG __stdcall Release() final;

    const Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    bool changed_ = false;
    std::mutex lock_;
};

} // namespace base

#endif // BASE_AUDIO_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H
