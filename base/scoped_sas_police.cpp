/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_sas_police.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/scoped_sas_police.h"

#include "base/logging.h"

static const DWORD kNone = 0;
static const DWORD kServices = 1;
static const DWORD kApplications = 2;
static const DWORD kServicesAndApplications = 3;

static const WCHAR kSoftwareSASGeneration[] = L"SoftwareSASGeneration";

ScopedSasPolice::ScopedSasPolice() :
    key_(nullptr),
    old_state_(kNone)
{
    DWORD disposition;

    LONG status = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                                  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                                  0,
                                  REG_NONE,
                                  REG_OPTION_NON_VOLATILE,
                                  KEY_READ | KEY_WRITE,
                                  nullptr,
                                  &key_,
                                  &disposition);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "RegCreateKeyExW() failed: " << status;
        return;
    }

    DWORD type = REG_DWORD;
    DWORD size = sizeof(old_state_);

    status = RegQueryValueExW(key_,
                              kSoftwareSASGeneration,
                              nullptr,
                              &type,
                              reinterpret_cast<LPBYTE>(&old_state_),
                              &size);

    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "RegQueryValueExW() failed: " << status;
        Close();
        return;
    }

    if (old_state_ >= kServices)
    {
        Close();
        return;
    }

    DWORD value = kServices;

    status = RegSetValueExW(key_,
                            kSoftwareSASGeneration,
                            0,
                            REG_DWORD,
                            reinterpret_cast<LPBYTE>(&value),
                            sizeof(value));
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "RegSetValueExW() failed: " << status;
        Close();
    }
}

ScopedSasPolice::~ScopedSasPolice()
{
    if (key_)
    {
        LONG status = RegSetValueExW(key_,
                                     kSoftwareSASGeneration,
                                     0,
                                     REG_DWORD,
                                     reinterpret_cast<LPBYTE>(&old_state_),
                                     sizeof(old_state_));
        if (status != ERROR_SUCCESS)
        {
            LOG(ERROR) << "RegSetValueExW() failed: " << status;
        }

        Close();
    }
}

void ScopedSasPolice::Close()
{
    if (key_)
    {
        RegCloseKey(key_);
        key_ = nullptr;
    }
}
