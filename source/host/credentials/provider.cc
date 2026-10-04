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

// <initguid.h> must precede the credential provider headers so the interface IIDs they declare
// are defined in this translation unit.
#include <initguid.h>

#include "host/credentials/provider.h"

#include <new>

#include "host/credentials/dll_main.h"
#include "host/credentials/logging.h"

namespace {

const FieldStatePair kFieldStatePairs[] =
{
    { CPFS_DISPLAY_IN_BOTH, CPFIS_NONE }, // SFI_LOGO
    { CPFS_DISPLAY_IN_BOTH, CPFIS_NONE }, // SFI_LABEL
};

const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR kFieldDescriptors[] =
{
    { SFI_LOGO,  CPFT_TILE_IMAGE, const_cast<LPWSTR>(L"Image") },
    { SFI_LABEL, CPFT_LARGE_TEXT, const_cast<LPWSTR>(L"Aspia") },
};

} // namespace

//--------------------------------------------------------------------------------------------------
Provider::Provider()
    : ipc_client_(this)
{
    LOG(L"Ctor");
    moduleAddRef();
}

//--------------------------------------------------------------------------------------------------
Provider::~Provider()
{
    LOG(L"Dtor");

    ipc_client_.stop();

    if (credential_)
        credential_->setDelegate(nullptr);

    secureClear(pending_domain_);
    secureClear(pending_username_);
    secureClear(pending_password_);
    moduleRelease();
}

//--------------------------------------------------------------------------------------------------
// static
HRESULT Provider::create(REFIID riid, void** ppv)
{
    Provider* provider = new(std::nothrow) Provider();
    if (!provider)
    {
        LOG(L"Provider allocation failed");
        return E_OUTOFMEMORY;
    }

    HRESULT hr = provider->QueryInterface(riid, ppv);
    provider->Release();
    return hr;
}

//--------------------------------------------------------------------------------------------------
ULONG Provider::AddRef()
{
    return ++ref_count_;
}

//--------------------------------------------------------------------------------------------------
ULONG Provider::Release()
{
    ULONG ref_count = --ref_count_;
    if (!ref_count)
        delete this;
    return ref_count;
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
    {
        LOG(L"Invalid argument");
        return E_POINTER;
    }

    static const QITAB qit[] = { QITABENT(Provider, ICredentialProvider), {0}, };
    return QISearch(this, qit, riid, ppv);
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD /* flags */)
{
    switch (cpus)
    {
        case CPUS_LOGON:
        case CPUS_UNLOCK_WORKSTATION:
        case CPUS_CREDUI:
            cpus_ = cpus;
            recreate_enumerated_credentials_ = true;

            if (!ipc_started_)
            {
                if (ipc_client_.start())
                    ipc_started_ = true;
                else
                    LOG(L"Failed to start the IPC client");
            }
            return S_OK;

        case CPUS_CHANGE_PASSWORD:
            LOG(L"Unsupported usage scenario: %d", cpus);
            return E_NOTIMPL;

        default:
            LOG(L"Invalid usage scenario: %d", cpus);
            return E_INVALIDARG;
    }
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::SetSerialization(CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION const * /* serialization */)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::Advise(ICredentialProviderEvents* events, UINT_PTR advise_context)
{
    events_ = events;
    advise_context_ = advise_context;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::UnAdvise()
{
    events_.Reset();
    advise_context_ = 0;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::GetFieldDescriptorCount(DWORD* count)
{
    if (!count)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *count = SFI_NUM_FIELDS;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::GetFieldDescriptorAt(DWORD index, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** descriptor)
{
    if (!descriptor)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *descriptor = nullptr;

    if (index >= SFI_NUM_FIELDS)
    {
        LOG(L"Invalid field index: %lu", index);
        return E_INVALIDARG;
    }

    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& source = kFieldDescriptors[index];
    DWORD struct_size = sizeof(**descriptor);

    ScopedCoMem<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR> result;
    result.reset(static_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(CoTaskMemAlloc(struct_size)));
    if (!result)
    {
        LOG(L"CoTaskMemAlloc failed");
        return E_OUTOFMEMORY;
    }

    SecureZeroMemory(result, struct_size);

    result->dwFieldID = source.dwFieldID;
    result->cpft = source.cpft;
    result->guidFieldType = source.guidFieldType;

    if (source.pszLabel)
    {
        HRESULT hr = SHStrDupW(source.pszLabel, &result->pszLabel);
        if (FAILED(hr))
        {
            LOG(L"SHStrDupW failed: 0x%08lX", hr);
            return hr;
        }
    }

    *descriptor = result.release();
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::GetCredentialCount(DWORD* count, DWORD* default_index, BOOL* auto_logon_with_default)
{
    if (!count || !default_index || !auto_logon_with_default)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *default_index = CREDENTIAL_PROVIDER_NO_DEFAULT;
    *auto_logon_with_default = FALSE;

    if (recreate_enumerated_credentials_)
    {
        recreate_enumerated_credentials_ = false;

        if (credential_)
            credential_->setDelegate(nullptr);

        credential_.Reset();

        if (cpus_ == CPUS_LOGON || cpus_ == CPUS_UNLOCK_WORKSTATION || cpus_ == CPUS_CREDUI)
        {
            credential_.Attach(new(std::nothrow) Credential());

            HRESULT hr = credential_ ?
                credential_->initialize(cpus_, kFieldDescriptors, kFieldStatePairs) : E_OUTOFMEMORY;
            if (SUCCEEDED(hr))
            {
                credential_->setDelegate(this);
            }
            else
            {
                LOG(L"Credential create failed: 0x%08lX", hr);
                credential_.Reset();
            }
        }
    }

    // No tile is shown until credentials arrive over IPC.
    if (credential_ && !pending_username_.empty())
    {
        *default_index = 0;
        *auto_logon_with_default = TRUE;
        *count = 1;
    }
    else
    {
        *count = 0;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Provider::GetCredentialAt(DWORD index, ICredentialProviderCredential** credential)
{
    if (!credential)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *credential = nullptr;

    if (index != 0)
    {
        LOG(L"Invalid credential index: %lu", index);
        return E_INVALIDARG;
    }

    if (!credential_)
    {
        LOG(L"No enumerated credential");
        return E_UNEXPECTED;
    }

    if (!pending_username_.empty())
    {
        HRESULT hr = credential_->setCredentials(pending_domain_.c_str(), pending_username_.c_str(),
                                                 pending_password_.c_str());
        if (FAILED(hr))
        {
            LOG(L"setCredentials failed: 0x%08lX", hr);
            return hr;
        }
    }

    return credential_.CopyTo(credential);
}

//--------------------------------------------------------------------------------------------------
void Provider::onCredentials(const wchar_t* domain, const wchar_t* username, const wchar_t* password)
{
    if (!domain || !username || !password)
    {
        LOG(L"Invalid argument");
        return;
    }

    secureClear(pending_domain_);
    secureClear(pending_username_);
    secureClear(pending_password_);

    try
    {
        pending_domain_ = domain;
        pending_username_ = username;
        pending_password_ = password;
    }
    catch (...)
    {
        LOG(L"Failed to store credentials");
        secureClear(pending_domain_);
        secureClear(pending_username_);
        secureClear(pending_password_);
        return;
    }

    LOG(L"Credentials received");

    if (events_)
    {
        HRESULT hr = events_->CredentialsChanged(advise_context_);
        if (FAILED(hr))
            LOG(L"CredentialsChanged failed: 0x%08lX", hr);
    }
}

//--------------------------------------------------------------------------------------------------
void Provider::onCredentialsRejected()
{
    secureClear(pending_domain_);
    secureClear(pending_username_);
    secureClear(pending_password_);
}
