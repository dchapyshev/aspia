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

#ifndef HOST_CREDENTIALS_CREDENTIAL_H
#define HOST_CREDENTIALS_CREDENTIAL_H

#include <unknwn.h>
#include <windows.h>

#include <atomic>

#include "host/credentials/win/utils.h"

class Credential final : public ICredentialProviderCredential
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void onCredentialsRejected() = 0;
    };

    Credential();
    ~Credential();

    HRESULT initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                       CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR const* field_descriptors,
                       FieldStatePair const* field_states);

    HRESULT setCredentials(PCWSTR domain, PCWSTR username, PCWSTR password);
    void setDelegate(Delegate* delegate) { delegate_ = delegate; }

    // IUnknown implementation.
    IFACEMETHODIMP_(ULONG) AddRef() final;
    IFACEMETHODIMP_(ULONG) Release() final;
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) final;

    // ICredentialProviderCredential implementation.
    IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* events) final;
    IFACEMETHODIMP UnAdvise() final;
    IFACEMETHODIMP SetSelected(BOOL* auto_logon) final;
    IFACEMETHODIMP SetDeselected() final;
    IFACEMETHODIMP GetFieldState(DWORD field_id, CREDENTIAL_PROVIDER_FIELD_STATE* field_state,
        CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* interactive_state) final;
    IFACEMETHODIMP GetStringValue(DWORD field_id, PWSTR* string) final;
    IFACEMETHODIMP GetBitmapValue(DWORD field_id, HBITMAP* bitmap) final;
    IFACEMETHODIMP GetCheckboxValue(DWORD field_id, BOOL* checked, PWSTR* label) final;
    IFACEMETHODIMP GetComboBoxValueCount(DWORD field_id, DWORD* item_count, DWORD* selected_item) final;
    IFACEMETHODIMP GetComboBoxValueAt(DWORD field_id, DWORD item_id, PWSTR* item) final;
    IFACEMETHODIMP GetSubmitButtonValue(DWORD field_id, DWORD* adjacent_to) final;
    IFACEMETHODIMP SetStringValue(DWORD field_id, PCWSTR text) final;
    IFACEMETHODIMP SetCheckboxValue(DWORD field_id, BOOL checked) final;
    IFACEMETHODIMP SetComboBoxSelectedValue(DWORD field_id, DWORD selected_item) final;
    IFACEMETHODIMP CommandLinkClicked(DWORD field_id) final;
    IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* response,
        CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization, PWSTR* optional_status_text,
        CREDENTIAL_PROVIDER_STATUS_ICON* optional_status_icon) final;
    IFACEMETHODIMP ReportResult(NTSTATUS status, NTSTATUS substatus, PWSTR* optional_status_text,
        CREDENTIAL_PROVIDER_STATUS_ICON* optional_status_icon) final;

private:
    std::atomic<ULONG> ref_count_ = 1;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus_ = CPUS_INVALID;
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR field_descriptors_[SFI_NUM_FIELDS] = {};
    FieldStatePair field_state_pairs_[SFI_NUM_FIELDS] = {};
    ScopedCoMem<wchar_t> field_strings_[SFI_NUM_FIELDS];
    ScopedCoMem<wchar_t> domain_;
    ScopedCoMem<wchar_t> username_;
    ScopedCoMem<wchar_t> password_;
    Delegate* delegate_ = nullptr;
};

#endif // HOST_CREDENTIALS_CREDENTIAL_H
