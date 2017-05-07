//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/scoped_sas_police.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/scoped_sas_police.h"
#include "base/logging.h"

namespace aspia {

static const DWORD kNone = 0;
static const DWORD kServices = 1;
static const DWORD kApplications = 2;
static const DWORD kServicesAndApplications = 3;

static const WCHAR kSoftwareSASGeneration[] = L"SoftwareSASGeneration";

ScopedSasPolice::ScopedSasPolice() :
    old_state_(kNone)
{
    LONG status = key_.Create(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                              KEY_READ | KEY_WRITE);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Create() failed: " << status;
        return;
    }

    status = key_.ReadValueDW(kSoftwareSASGeneration, &old_state_);
    if (status != ERROR_SUCCESS)
    {
        // The previous state is not defined.
        // Consider that the software generation of SAS has been disabled.
        old_state_ = kNone;
    }

    if (old_state_ >= kServices)
    {
        key_.Close();
        return;
    }

    status = key_.WriteValue(kSoftwareSASGeneration, kServices);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "WriteValue() failed: " << status;
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
            LOG(WARNING) << "WriteValue() failed: " << status;
        }
    }
}

} // namespace aspia
