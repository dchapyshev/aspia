//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_impersonator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_impersonator.h"
#include "base/logging.h"

namespace aspia {

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
        LOG(ERROR) << "ImpersonateLoggedOnUser() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    impersonated_ = true;
    return true;
}

bool ScopedImpersonator::ImpersonateAnonymous()
{
    if (!ImpersonateAnonymousToken(GetCurrentThread()))
    {
        LOG(ERROR) << "ImpersonateAnonymousToken() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    impersonated_ = true;
    return true;
}

bool ScopedImpersonator::ImpersonateNamedPipeClient(HANDLE named_pipe)
{
    if (!::ImpersonateNamedPipeClient(named_pipe))
    {
        LOG(ERROR) << "ImpersonateNamedPipeClient() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    impersonated_ = true;
    return true;
}

} // namespace aspia
