//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_impersonator.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_IMPERSONATOR_H
#define _ASPIA_BASE__SCOPED_IMPERSONATOR_H

#include "aspia_config.h"

#include "base/macros.h"

namespace aspia {

class ScopedImpersonator
{
public:
    ScopedImpersonator();
    ~ScopedImpersonator();

    bool ImpersonateLoggedOnUser(HANDLE user_token);
    bool ImpersonateAnonymous();
    bool ImpersonateNamedPipeClient(HANDLE named_pipe);

private:
    bool impersonated_;

    DISALLOW_COPY_AND_ASSIGN(ScopedImpersonator);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_IMPERSONATOR_H
