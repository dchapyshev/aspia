//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/sas_injector.h"

#include "base/logging.h"
#include "base/win/registry.h"

#include <sas.h>

namespace host {

namespace {

const wchar_t kSoftwareSASGenerationPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";

const wchar_t kSoftwareSASGeneration[] = L"SoftwareSASGeneration";

class ScopedSasPolicy
{
public:
    ScopedSasPolicy();
    ~ScopedSasPolicy();

    static const DWORD kNone = 0;
    static const DWORD kApplications = 2;

private:
    base::win::RegistryKey key_;
    DWORD old_state_ = kNone;

    DISALLOW_COPY_AND_ASSIGN(ScopedSasPolicy);
};

ScopedSasPolicy::ScopedSasPolicy()
{
    LONG status = key_.create(HKEY_LOCAL_MACHINE,
                              kSoftwareSASGenerationPath,
                              KEY_READ | KEY_WRITE);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "key.create failed: " << base::SystemError::toString(status);
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
            LOG(LS_WARNING) << "key.writeValue failed: " << base::SystemError::toString(status);
            key_.close();
        }
    }
}

ScopedSasPolicy::~ScopedSasPolicy()
{
    if (!key_.isValid())
        return;

    LONG status = key_.writeValue(kSoftwareSASGeneration, old_state_);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "key.writeValue failed: " << base::SystemError::toString(status);
        return;
    }
}

} // namespace

void injectSAS()
{
    ScopedSasPolicy sas_policy;
    SendSAS(FALSE);
}

} // namespace host
