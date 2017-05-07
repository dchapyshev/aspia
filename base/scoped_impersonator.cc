//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_impersonator.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_impersonator.h"
#include "base/scoped_object.h"
#include "base/logging.h"

#include <wtsapi32.h>

namespace aspia {

static DWORD kInvalidSessionId = 0xFFFFFFFF;

ScopedImpersonator::ScopedImpersonator() :
    impersonated_(false)
{
    // Nothing
}

ScopedImpersonator::~ScopedImpersonator()
{
    if (impersonated_)
    {
        BOOL ret = RevertToSelf();
        CHECK(ret);
    }
}

bool ScopedImpersonator::ImpersonateLoggedOnUser(HANDLE user_token)
{
    if (!::ImpersonateLoggedOnUser(user_token))
    {
        LOG(ERROR) << "ImpersonateLoggedOnUser() failed: " << GetLastError();
        return false;
    }

    impersonated_ = true;

    return true;
}

bool ScopedImpersonator::ImpersonateAnonymous()
{
    if (!ImpersonateAnonymousToken(GetCurrentThread()))
    {
        LOG(ERROR) << "ImpersonateAnonymousToken() failed: " << GetLastError();
        return false;
    }

    impersonated_ = true;

    return true;
}

bool ScopedImpersonator::ImpersonateNamedPipeClient(HANDLE named_pipe)
{
    if (!::ImpersonateNamedPipeClient(named_pipe))
    {
        LOG(ERROR) << "ImpersonateNamedPipeClient() failed: " << GetLastError();
        return false;
    }

    impersonated_ = true;

    return true;
}

} // namespace aspia
