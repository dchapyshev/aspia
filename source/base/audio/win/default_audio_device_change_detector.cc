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

#include "base/audio/win/default_audio_device_change_detector.h"

#include "base/logging.h"

#include <comdef.h>
#include <unknwn.h>

namespace base {

//--------------------------------------------------------------------------------------------------
DefaultAudioDeviceChangeDetector::DefaultAudioDeviceChangeDetector(
    const Microsoft::WRL::ComPtr<IMMDeviceEnumerator>& enumerator)
    : enumerator_(enumerator)
{
    DCHECK(enumerator_);
    _com_error hr = enumerator_->RegisterEndpointNotificationCallback(this);
    if (FAILED(hr.Error()))
    {
        // We cannot predict which kind of error the API may return, but this is not a fatal error.
        LOG(ERROR) << "Failed to register IMMNotificationClient, we may not be "
                      "able to detect the new default audio device. Error:" << hr;
    }
}

//--------------------------------------------------------------------------------------------------
DefaultAudioDeviceChangeDetector::~DefaultAudioDeviceChangeDetector()
{
    enumerator_->UnregisterEndpointNotificationCallback(this);
}

//--------------------------------------------------------------------------------------------------
bool DefaultAudioDeviceChangeDetector::getAndReset()
{
    bool result = false;
    {
        std::scoped_lock lock(lock_);
        result = changed_;
        changed_ = false;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
HRESULT DefaultAudioDeviceChangeDetector::OnDefaultDeviceChanged(
    EDataFlow /* flow */, ERole /* role */, LPCWSTR /* pwstrDefaultDevice */)
{
    {
        std::scoped_lock lock(lock_);
        changed_ = true;
    }
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT DefaultAudioDeviceChangeDetector::QueryInterface(REFIID iid, void** object)
{
    if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient))
    {
        *object = static_cast<IMMNotificationClient*>(this);
        return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
}

//--------------------------------------------------------------------------------------------------
HRESULT DefaultAudioDeviceChangeDetector::OnDeviceAdded(LPCWSTR /* pwstrDeviceId */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT DefaultAudioDeviceChangeDetector::OnDeviceRemoved(LPCWSTR /* pwstrDeviceId */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT DefaultAudioDeviceChangeDetector::OnDeviceStateChanged(
    LPCWSTR /* pwstrDeviceId */, DWORD /* dwNewState */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT DefaultAudioDeviceChangeDetector::OnPropertyValueChanged(
    LPCWSTR /* pwstrDeviceId */, const PROPERTYKEY /* key */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
ULONG DefaultAudioDeviceChangeDetector::AddRef() { return 1; }

//--------------------------------------------------------------------------------------------------
ULONG DefaultAudioDeviceChangeDetector::Release() { return 1; }

} // namespace base
