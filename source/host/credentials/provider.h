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

#ifndef HOST_CREDENTIALS_PROVIDER_H
#define HOST_CREDENTIALS_PROVIDER_H

#include <windows.h>
#include <wrl/client.h>

#include <atomic>
#include <string>

#include "host/credentials/credential.h"
#include "host/credentials/ipc_client.h"

class Provider final : public ICredentialProvider,
                       public IpcClient::Delegate,
                       public Credential::Delegate
{
public:
    ~Provider() final;

    static HRESULT create(REFIID riid, void** ppv);

private:
    Provider();

    // IUnknown implementation.
    IFACEMETHODIMP_(ULONG) AddRef() final;
    IFACEMETHODIMP_(ULONG) Release() final;
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) final;

    // ICredentialProvider implementation.
    IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD flags) final;
    IFACEMETHODIMP SetSerialization(CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION const* serialization) final;
    IFACEMETHODIMP Advise(ICredentialProviderEvents* events, UINT_PTR advise_context) final;
    IFACEMETHODIMP UnAdvise() final;
    IFACEMETHODIMP GetFieldDescriptorCount(DWORD* count) final;
    IFACEMETHODIMP GetFieldDescriptorAt(DWORD index, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd) final;
    IFACEMETHODIMP GetCredentialCount(DWORD* count, DWORD* default_index, BOOL* auto_logon_with_default) final;
    IFACEMETHODIMP GetCredentialAt(DWORD index, ICredentialProviderCredential** credential) final;

    // IpcClient::Delegate implementation.
    void onCredentials(const wchar_t* domain, const wchar_t* username,
                       const wchar_t* password) final;

    // Credential::Delegate implementation.
    void onCredentialsRejected() final;

    std::atomic<ULONG> ref_count_ = 1;
    Microsoft::WRL::ComPtr<Credential> credential_;
    Microsoft::WRL::ComPtr<ICredentialProviderEvents> events_;
    UINT_PTR advise_context_ = 0;
    bool recreate_enumerated_credentials_ = false;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus_ = CPUS_INVALID;

    IpcClient ipc_client_;
    bool ipc_started_ = false;

    std::wstring pending_domain_;
    std::wstring pending_username_;
    std::wstring pending_password_;
};

#endif // HOST_CREDENTIALS_PROVIDER_H
