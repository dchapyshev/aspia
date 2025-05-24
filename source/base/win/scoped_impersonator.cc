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

#include "base/win/scoped_impersonator.h"
#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedImpersonator::ScopedImpersonator() = default;

//--------------------------------------------------------------------------------------------------
ScopedImpersonator::~ScopedImpersonator()
{
    revertToSelf();
}

//--------------------------------------------------------------------------------------------------
bool ScopedImpersonator::loggedOnUser(HANDLE user_token)
{
    if (!ImpersonateLoggedOnUser(user_token))
    {
        PLOG(LS_ERROR) << "ImpersonateLoggedOnUser failed";
        return false;
    }

    impersonated_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScopedImpersonator::anonymous()
{
    if (!ImpersonateAnonymousToken(GetCurrentThread()))
    {
        PLOG(LS_ERROR) << "ImpersonateAnonymousToken failed";
        return false;
    }

    impersonated_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScopedImpersonator::namedPipeClient(HANDLE named_pipe)
{
    if (!ImpersonateNamedPipeClient(named_pipe))
    {
        PLOG(LS_ERROR) << "ImpersonateNamedPipeClient failed";
        return false;
    }

    impersonated_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScopedImpersonator::revertToSelf()
{
    if (!impersonated_)
        return;

    BOOL ret = RevertToSelf();
    CHECK(ret);

    impersonated_ = false;
}

} // namespace base
