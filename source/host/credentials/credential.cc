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

#include "host/credentials/credential.h"

#include <ntsecapi.h>
#define SECURITY_WIN32
#include <security.h>
#include <intsafe.h>
#include <wincred.h>

#include "host/credentials/dll_main.h"
#include "host/credentials/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
HRESULT packUnicodeString(const UNICODE_STRING& source, PWSTR buffer, UNICODE_STRING* unicode_string)
{
    if (!buffer || !unicode_string)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    unicode_string->Length = source.Length;
    unicode_string->MaximumLength = source.Length;
    unicode_string->Buffer = buffer;
    CopyMemory(unicode_string->Buffer, source.Buffer, unicode_string->Length);
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT lsaInitString(PSTRING destination_string, PCSTR source_string)
{
    if (!destination_string || !source_string)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    size_t length = strlen(source_string);
    USHORT short_length = 0;
    HRESULT hr = SizeTToUShort(length, &short_length);
    if (FAILED(hr))
    {
        LOG(L"SizeTToUShort failed: 0x%08lX", hr);
        return hr;
    }

    destination_string->Buffer = const_cast<PCHAR>(source_string);
    destination_string->Length = short_length;
    destination_string->MaximumLength = destination_string->Length + 1;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT toUnicodeString(PWSTR text, UNICODE_STRING* unicode_string)
{
    if (!text || !unicode_string)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    size_t string_length = wcslen(text);
    USHORT char_count = 0;
    HRESULT hr = SizeTToUShort(string_length, &char_count);
    if (FAILED(hr))
    {
        LOG(L"SizeTToUShort failed: 0x%08lX", hr);
        return hr;
    }

    USHORT size = 0;
    hr = SizeTToUShort(sizeof(wchar_t), &size);
    if (FAILED(hr))
    {
        LOG(L"SizeTToUShort failed: 0x%08lX", hr);
        return hr;
    }

    hr = UShortMult(char_count, size, &(unicode_string->Length));
    if (FAILED(hr))
    {
        LOG(L"UShortMult failed: 0x%08lX", hr);
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    unicode_string->MaximumLength = unicode_string->Length;
    unicode_string->Buffer = text;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT buildUnlockLogon(PWSTR domain, PWSTR username, PWSTR password,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, KERB_INTERACTIVE_UNLOCK_LOGON* unlock_logon)
{
    if (!domain || !username || !password || !unlock_logon)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    KERB_INTERACTIVE_UNLOCK_LOGON result;
    SecureZeroMemory(&result, sizeof(result));

    KERB_INTERACTIVE_LOGON* logon = &result.Logon;

    HRESULT hr = toUnicodeString(domain, &logon->LogonDomainName);
    if (FAILED(hr))
    {
        LOG(L"toUnicodeString failed for domain: 0x%08lX", hr);
        return hr;
    }

    hr = toUnicodeString(username, &logon->UserName);
    if (FAILED(hr))
    {
        LOG(L"toUnicodeString failed for username: 0x%08lX", hr);
        return hr;
    }

    hr = toUnicodeString(password, &logon->Password);
    if (FAILED(hr))
    {
        LOG(L"toUnicodeString failed for password: 0x%08lX", hr);
        return hr;
    }

    switch (cpus)
    {
        case CPUS_UNLOCK_WORKSTATION:
            logon->MessageType = KerbWorkstationUnlockLogon;
            break;

        case CPUS_LOGON:
            logon->MessageType = KerbInteractiveLogon;
            break;

        case CPUS_CREDUI:
            logon->MessageType = static_cast<KERB_LOGON_SUBMIT_TYPE>(0);
            break;

        default:
            LOG(L"Unexpected usage scenario: %d", cpus);
            return E_FAIL;
    }

    CopyMemory(unlock_logon, &result, sizeof(*unlock_logon));
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT packUnlockLogon(const KERB_INTERACTIVE_UNLOCK_LOGON& source, BYTE** out_buffer, DWORD* out_size)
{
    if (!out_buffer || !out_size)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    const KERB_INTERACTIVE_LOGON* logon_in = &source.Logon;

    DWORD size = sizeof(source) + logon_in->LogonDomainName.Length + logon_in->UserName.Length +
        logon_in->Password.Length;

    ScopedCoMem<KERB_INTERACTIVE_UNLOCK_LOGON> unlock_logon;
    unlock_logon.reset(static_cast<KERB_INTERACTIVE_UNLOCK_LOGON*>(CoTaskMemAlloc(size)));
    if (!unlock_logon)
    {
        LOG(L"CoTaskMemAlloc failed");
        return E_OUTOFMEMORY;
    }

    SecureZeroMemory(unlock_logon.get(), size);

    BYTE* base = reinterpret_cast<BYTE*>(unlock_logon.get());
    BYTE* buffer = base + sizeof(KERB_INTERACTIVE_UNLOCK_LOGON);
    KERB_INTERACTIVE_LOGON* logon_out = &unlock_logon->Logon;

    logon_out->MessageType = logon_in->MessageType;

    HRESULT hr = packUnicodeString(logon_in->LogonDomainName, reinterpret_cast<PWSTR>(buffer),
                                   &logon_out->LogonDomainName);
    if (FAILED(hr))
    {
        LOG(L"packUnicodeString failed for domain: 0x%08lX", hr);
        return hr;
    }
    logon_out->LogonDomainName.Buffer = reinterpret_cast<PWSTR>(buffer - base);
    buffer += logon_out->LogonDomainName.Length;

    hr = packUnicodeString(logon_in->UserName, reinterpret_cast<PWSTR>(buffer), &logon_out->UserName);
    if (FAILED(hr))
    {
        LOG(L"packUnicodeString failed for username: 0x%08lX", hr);
        return hr;
    }
    logon_out->UserName.Buffer = reinterpret_cast<PWSTR>(buffer - base);
    buffer += logon_out->UserName.Length;

    hr = packUnicodeString(logon_in->Password, reinterpret_cast<PWSTR>(buffer), &logon_out->Password);
    if (FAILED(hr))
    {
        LOG(L"packUnicodeString failed for password: 0x%08lX", hr);
        return hr;
    }
    logon_out->Password.Buffer = reinterpret_cast<PWSTR>(buffer - base);

    *out_buffer = reinterpret_cast<BYTE*>(unlock_logon.release());
    *out_size = size;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT lookupAuthPackage(ULONG* auth_package)
{
    if (!auth_package)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    HANDLE lsa = nullptr;
    NTSTATUS status = LsaConnectUntrusted(&lsa);
    if (FAILED(HRESULT_FROM_NT(status)))
    {
        LOG(L"LsaConnectUntrusted failed: 0x%08lX", status);
        return HRESULT_FROM_NT(status);
    }

    ULONG package = 0;
    LSA_STRING kerberos_name = {};
    HRESULT hr = lsaInitString(&kerberos_name, NEGOSSP_NAME_A);
    if (FAILED(hr))
    {
        LOG(L"lsaInitString failed: 0x%08lX", hr);
        LsaDeregisterLogonProcess(lsa);
        return hr;
    }

    status = LsaLookupAuthenticationPackage(lsa, &kerberos_name, &package);
    LsaDeregisterLogonProcess(lsa);
    if (FAILED(HRESULT_FROM_NT(status)))
    {
        LOG(L"LsaLookupAuthenticationPackage failed: 0x%08lX", status);
        return HRESULT_FROM_NT(status);
    }

    *auth_package = package;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT protectIfNecessaryAndCopyPassword(PCWSTR password, CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    PWSTR* protected_password)
{
    if (!protected_password)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *protected_password = nullptr;

    if (!password || !*password)
        return SHStrDupW(L"", protected_password);

    ScopedCoMem<wchar_t> password_copy;
    HRESULT hr = SHStrDupW(password, &password_copy);
    if (FAILED(hr))
    {
        LOG(L"SHStrDupW failed: 0x%08lX", hr);
        return hr;
    }

    bool already_encrypted = false;
    CRED_PROTECTION_TYPE protection_type = CredUnprotected;
    if (CredIsProtectedW(password_copy, &protection_type) && protection_type != CredUnprotected)
        already_encrypted = true;

    if (CPUS_CREDUI == cpus || already_encrypted)
        return SHStrDupW(password_copy, protected_password);

    DWORD protected_length = 0;
    if (CredProtectW(FALSE, password_copy, static_cast<DWORD>(wcslen(password_copy)) + 1, nullptr,
                     &protected_length, nullptr))
    {
        LOG(L"CredProtectW unexpectedly succeeded on the size query");
        return E_UNEXPECTED;
    }

    DWORD error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER || protected_length == 0)
    {
        LOG(L"CredProtectW size query failed: %lu", error);
        return HRESULT_FROM_WIN32(error);
    }

    ScopedCoMem<wchar_t> protected_buffer;
    protected_buffer.reset(static_cast<PWSTR>(CoTaskMemAlloc(protected_length * sizeof(wchar_t))));
    if (!protected_buffer)
    {
        LOG(L"CoTaskMemAlloc failed");
        return E_OUTOFMEMORY;
    }

    if (!CredProtectW(FALSE, password_copy, static_cast<DWORD>(wcslen(password_copy)) + 1,
                      protected_buffer, &protected_length, nullptr))
    {
        error = GetLastError();
        LOG(L"CredProtectW failed: %lu", error);
        return HRESULT_FROM_WIN32(error);
    }

    *protected_password = protected_buffer.release();
    return S_OK;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Credential::Credential()
{
    LOG(L"Ctor");
    moduleAddRef();
}

//--------------------------------------------------------------------------------------------------
Credential::~Credential()
{
    LOG(L"Dtor");

    secureReset(domain_);
    secureReset(username_);
    secureReset(password_);

    for (int i = 0; i < ARRAYSIZE(field_descriptors_); ++i)
        CoTaskMemFree(field_descriptors_[i].pszLabel);
    moduleRelease();
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR const* field_descriptors, FieldStatePair const* field_states)
{
    if (!field_descriptors || !field_states)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    if (cpus != CPUS_LOGON && cpus != CPUS_UNLOCK_WORKSTATION && cpus != CPUS_CREDUI)
    {
        LOG(L"Unsupported usage scenario: %d", cpus);
        return E_INVALIDARG;
    }

    cpus_ = cpus;

    HRESULT hr = S_OK;
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(field_descriptors_); ++i)
    {
        field_state_pairs_[i] = field_states[i];

        const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& source = field_descriptors[i];

        CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR descriptor;
        SecureZeroMemory(&descriptor, sizeof(descriptor));

        descriptor.dwFieldID = source.dwFieldID;
        descriptor.cpft = source.cpft;
        descriptor.guidFieldType = source.guidFieldType;

        if (source.pszLabel)
        {
            hr = SHStrDupW(source.pszLabel, &descriptor.pszLabel);
            if (FAILED(hr))
            {
                LOG(L"SHStrDupW failed: 0x%08lX", hr);
                break;
            }
        }

        field_descriptors_[i] = descriptor;
    }

    if (SUCCEEDED(hr))
        hr = SHStrDupW(L"Aspia", &field_strings_[SFI_LABEL]);

    if (FAILED(hr))
        LOG(L"Credential initialization failed: 0x%08lX", hr);

    return hr;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::setCredentials(PCWSTR domain, PCWSTR username, PCWSTR password)
{
    ScopedCoMem<wchar_t> domain_copy;
    HRESULT hr = SHStrDupW(domain ? domain : L"", &domain_copy);
    if (FAILED(hr))
    {
        LOG(L"SHStrDupW failed for domain: 0x%08lX", hr);
        return hr;
    }

    ScopedCoMem<wchar_t> username_copy;
    hr = SHStrDupW(username ? username : L"", &username_copy);
    if (FAILED(hr))
    {
        LOG(L"SHStrDupW failed for username: 0x%08lX", hr);
        return hr;
    }

    ScopedCoMem<wchar_t> password_copy;
    hr = SHStrDupW(password ? password : L"", &password_copy);
    if (FAILED(hr))
    {
        LOG(L"SHStrDupW failed for password: 0x%08lX", hr);
        return hr;
    }

    secureReset(domain_);
    secureReset(username_);
    secureReset(password_);

    domain_.reset(domain_copy.release());
    username_.reset(username_copy.release());
    password_.reset(password_copy.release());
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
ULONG Credential::AddRef()
{
    return ++ref_count_;
}

//--------------------------------------------------------------------------------------------------
ULONG Credential::Release()
{
    ULONG ref_count = --ref_count_;
    if (!ref_count)
        delete this;
    return ref_count;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
    {
        LOG(L"Invalid argument");
        return E_POINTER;
    }

    static const QITAB qit[] = { QITABENT(Credential, ICredentialProviderCredential), {0}, };
    return QISearch(this, qit, riid, ppv);
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::Advise(ICredentialProviderCredentialEvents* /* events */)
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::UnAdvise()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::SetSelected(BOOL* auto_logon)
{
    if (!auto_logon)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *auto_logon = FALSE;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::SetDeselected()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetFieldState(DWORD field_id, CREDENTIAL_PROVIDER_FIELD_STATE* field_state,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* interactive_state)
{
    if (!field_state || !interactive_state)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    if (field_id >= ARRAYSIZE(field_state_pairs_))
    {
        LOG(L"Invalid field id: %lu", field_id);
        return E_INVALIDARG;
    }

    *field_state = field_state_pairs_[field_id].state;
    *interactive_state = field_state_pairs_[field_id].interactive_state;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetStringValue(DWORD field_id, PWSTR* string)
{
    if (!string)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *string = nullptr;

    if (field_id >= ARRAYSIZE(field_descriptors_))
    {
        LOG(L"Invalid field id: %lu", field_id);
        return E_INVALIDARG;
    }

    PCWSTR value = field_strings_[field_id];
    return SHStrDupW(value ? value : L"", string);
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetBitmapValue(DWORD field_id, HBITMAP* bitmap)
{
    if (!bitmap)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *bitmap = nullptr;

    if (SFI_LOGO != field_id)
    {
        LOG(L"Invalid field id: %lu", field_id);
        return E_INVALIDARG;
    }

    HBITMAP loaded_bitmap = LoadBitmap(g_instance, L"LOGO");
    if (loaded_bitmap == nullptr)
    {
        DWORD error = GetLastError();
        LOG(L"LoadBitmap failed: %lu", error);
        return HRESULT_FROM_WIN32(error);
    }

    *bitmap = loaded_bitmap;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetCheckboxValue(DWORD /* field_id */, BOOL* checked, PWSTR* label)
{
    if (!checked || !label)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *checked = FALSE;
    *label = nullptr;
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetComboBoxValueCount(DWORD /* field_id */, DWORD* item_count, DWORD* selected_item)
{
    if (!item_count || !selected_item)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *item_count = 0;
    *selected_item = 0;
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetComboBoxValueAt(DWORD /* field_id */, DWORD /* item_id */, PWSTR* item)
{
    if (!item)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *item = nullptr;
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetSubmitButtonValue(DWORD /* field_id */, DWORD* adjacent_to)
{
    if (!adjacent_to)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *adjacent_to = 0;
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::SetStringValue(DWORD /* field_id */, PCWSTR /* text */)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::SetCheckboxValue(DWORD /* field_id */, BOOL /* checked */)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::SetComboBoxSelectedValue(DWORD /* field_id */, DWORD /* selected_item */)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::CommandLinkClicked(DWORD /* field_id */)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* response,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization, PWSTR* optional_status_text,
    CREDENTIAL_PROVIDER_STATUS_ICON* optional_status_icon)
{
    if (!response || !serialization || !optional_status_text || !optional_status_icon)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *response = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
    *optional_status_text = nullptr;
    *optional_status_icon = CPSI_NONE;
    SecureZeroMemory(serialization, sizeof(*serialization));

    // No credentials have been received from the host service yet.
    if (!username_ || !password_)
        return S_OK;

    ScopedCoMem<wchar_t> protected_password;
    HRESULT hr = protectIfNecessaryAndCopyPassword(password_, cpus_, &protected_password);
    if (FAILED(hr))
    {
        LOG(L"protectIfNecessaryAndCopyPassword failed: 0x%08lX", hr);
        return hr;
    }

    ULONG auth_package = 0;
    hr = lookupAuthPackage(&auth_package);
    if (FAILED(hr))
    {
        LOG(L"lookupAuthPackage failed: 0x%08lX", hr);
        return hr;
    }

    KERB_INTERACTIVE_UNLOCK_LOGON unlock_logon = {};
    hr = buildUnlockLogon(domain_, username_, protected_password, cpus_, &unlock_logon);
    if (FAILED(hr))
    {
        LOG(L"buildUnlockLogon failed: 0x%08lX", hr);
        return hr;
    }

    hr = packUnlockLogon(unlock_logon, &serialization->rgbSerialization, &serialization->cbSerialization);
    if (FAILED(hr))
    {
        LOG(L"packUnlockLogon failed: 0x%08lX", hr);
        return hr;
    }

    serialization->ulAuthenticationPackage = auth_package;
    serialization->clsidCredentialProvider = CLSID_AspiaCredentials;
    *response = CPGSR_RETURN_CREDENTIAL_FINISHED;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT Credential::ReportResult(NTSTATUS status, NTSTATUS substatus, PWSTR* optional_status_text,
    CREDENTIAL_PROVIDER_STATUS_ICON* optional_status_icon)
{
    if (!optional_status_text || !optional_status_icon)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *optional_status_text = nullptr;
    *optional_status_icon = CPSI_NONE;

    if (FAILED(HRESULT_FROM_NT(status)))
    {
        LOG(L"Logon reported failure: status=0x%08lX substatus=0x%08lX", status, substatus);

        secureReset(domain_);
        secureReset(username_);
        secureReset(password_);

        if (delegate_)
            delegate_->onCredentialsRejected();
    }

    return S_OK;
}
