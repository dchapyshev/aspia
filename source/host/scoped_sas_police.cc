//
// PROJECT:         Aspia
// FILE:            host/scoped_sas_police.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/scoped_sas_police.h"
#include "base/logging.h"

namespace aspia {

namespace {

constexpr WCHAR kSoftwareSASGenerationPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";

constexpr WCHAR kSoftwareSASGeneration[] = L"SoftwareSASGeneration";

} // namespace

ScopedSasPolice::ScopedSasPolice()
{
    LONG status = key_.Create(HKEY_LOCAL_MACHINE, kSoftwareSASGenerationPath, KEY_READ | KEY_WRITE);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Create() failed: " << status;
        return;
    }

    status = key_.ReadValueDW(kSoftwareSASGeneration, &old_state_);
    if (status != ERROR_SUCCESS)
    {
        // The previous state is not defined.
        // Consider that the software generation of SAS has been disabled.
        old_state_ = STATE_NONE;
    }

    if (old_state_ >= STATE_SERVICES)
    {
        key_.Close();
        return;
    }

    status = key_.WriteValue(kSoftwareSASGeneration, STATE_SERVICES);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "WriteValue() failed: " << status;
        key_.Close();
    }
}

ScopedSasPolice::~ScopedSasPolice()
{
    if (key_.IsValid())
    {
        LONG status = key_.WriteValue(kSoftwareSASGeneration, old_state_);
        if (status != ERROR_SUCCESS)
        {
            LOG(LS_WARNING) << "WriteValue() failed: " << status;
        }
    }
}

} // namespace aspia
