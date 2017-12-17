//
// PROJECT:         Aspia
// FILE:            base/scoped_impersonator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_IMPERSONATOR_H
#define _ASPIA_BASE__SCOPED_IMPERSONATOR_H

#include "base/macros.h"

namespace aspia {

class ScopedImpersonator
{
public:
    ScopedImpersonator() = default;
    ~ScopedImpersonator();

    bool ImpersonateLoggedOnUser(HANDLE user_token);
    bool ImpersonateAnonymous();
    bool ImpersonateNamedPipeClient(HANDLE named_pipe);

private:
    bool impersonated_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedImpersonator);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_IMPERSONATOR_H
