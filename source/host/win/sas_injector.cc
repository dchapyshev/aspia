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

#include "host/win/sas_injector.h"

#include "base/logging.h"
#include "base/win/registry.h"

#include <sas.h>

namespace host {

namespace {

const char kSoftwareSASGenerationPath[] =
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";

const char kSoftwareSASGeneration[] = "SoftwareSASGeneration";

class ScopedSasPolicy
{
public:
    ScopedSasPolicy();
    ~ScopedSasPolicy();

    static const DWORD kNone = 0;
    static const DWORD kApplications = 2;

private:
    base::RegistryKey key_;
    DWORD old_state_ = kNone;

    Q_DISABLE_COPY(ScopedSasPolicy)
};

//--------------------------------------------------------------------------------------------------
ScopedSasPolicy::ScopedSasPolicy()
{
    LONG status = key_.create(HKEY_LOCAL_MACHINE,
                              kSoftwareSASGenerationPath,
                              KEY_READ | KEY_WRITE);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "key.create failed:"
                   << base::SystemError::toString(static_cast<DWORD>(status));
        return;
    }

    status = key_.readValueDW(kSoftwareSASGeneration, &old_state_);
    if (status != ERROR_SUCCESS)
    {
        // The previous state is not defined.
        // Consider that the software generation of SAS has been disabled.
        old_state_ = kNone;
    }

    if (old_state_ < kApplications)
    {
        status = key_.writeValue(kSoftwareSASGeneration, kApplications);
        if (status != ERROR_SUCCESS)
        {
            LOG(ERROR) << "key.writeValue failed:"
                       << base::SystemError::toString(static_cast<DWORD>(status));
            key_.close();
        }
    }
}

//--------------------------------------------------------------------------------------------------
ScopedSasPolicy::~ScopedSasPolicy()
{
    if (!key_.isValid())
        return;

    LONG status = key_.writeValue(kSoftwareSASGeneration, old_state_);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "key.writeValue failed:"
                   << base::SystemError::toString(static_cast<DWORD>(status));
        return;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
void injectSAS()
{
    HMODULE sas_dll = LoadLibraryExW(L"sas.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (sas_dll)
    {
        auto send_sas_proc = reinterpret_cast<decltype(SendSAS)*>(GetProcAddress(sas_dll, "SendSAS"));
        if (send_sas_proc)
        {
            ScopedSasPolicy sas_policy;
            send_sas_proc(FALSE);
        }
        else
        {
            PLOG(ERROR) << "Unable to load SendSAS function";
        }

        FreeLibrary(sas_dll);
    }
    else
    {
        PLOG(ERROR) << "Unable to load sas.dll";
    }
}

} // namespace host
