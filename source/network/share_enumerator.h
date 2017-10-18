//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/share_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SHARE_ENUMERATOR_H
#define _ASPIA_NETWORK__SHARE_ENUMERATOR_H

#include "base/macros.h"

#include <lm.h>
#include <string>

namespace aspia {

class ShareEnumerator
{
public:
    ShareEnumerator();
    ~ShareEnumerator();

    bool IsAtEnd() const;
    void Advance();

    std::string GetName() const;

    enum class Type
    {
        UNKNOWN   = 0,
        DISK      = 1,
        PRINTER   = 2,
        DEVICE    = 3,
        IPC       = 4,
        SPECIAL   = 5,
        TEMPORARY = 6
    };

    Type GetType() const;
    std::string GetDescription() const;
    std::string GetLocalPath() const;
    uint32_t GetCurrentUses() const;
    uint32_t GetMaximumUses() const;

private:
    PSHARE_INFO_502 share_info_ = nullptr;
    DWORD total_entries_ = 0;
    DWORD current_entry_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ShareEnumerator);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SHARE_ENUMERATOR_H
