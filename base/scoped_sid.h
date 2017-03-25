//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_sid.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_SID_H
#define _ASPIA_BASE__SCOPED_SID_H

#include "aspia_config.h"
#include "base/macros.h"

namespace aspia {

class ScopedSid
{
public:
    ScopedSid() :
        sid_(nullptr)
    {
        // Nothing
    }

    explicit ScopedSid(PSID sid) :
        sid_(sid)
    {
        // Nothing
    }

    ScopedSid(ScopedSid&& other)
    {
        sid_ = other.sid_;
        other.sid_ = nullptr;
    }

    ~ScopedSid()
    {
        Close();
    }

    PSID Get()
    {
        return sid_;
    }

    void Set(PSID sid)
    {
        Close();
        sid_ = sid;
    }

    PSID Release()
    {
        PSID sid = sid_;
        sid_ = nullptr;
        return sid;
    }

    PSID* Recieve()
    {
        Close();
        return &sid_;
    }

    bool IsValid() const
    {
        return (sid_ != nullptr);
    }

    ScopedSid& operator=(PSID other)
    {
        Set(other);
        return *this;
    }

    ScopedSid& operator=(ScopedSid&& other)
    {
        Close();
        sid_ = other.sid_;
        other.sid_ = nullptr;
        return *this;
    }

    operator PSID()
    {
        return sid_;
    }

private:
    void Close()
    {
        if (IsValid())
        {
            FreeSid(sid_);
            sid_ = nullptr;
        }
    }

private:
    PSID sid_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSid);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_SID_H
