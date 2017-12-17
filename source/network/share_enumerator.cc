//
// PROJECT:         Aspia
// FILE:            network/share_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/share_enumerator.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

ShareEnumerator::ShareEnumerator()
{
    DWORD entries_read = 0;

    DWORD error_code = NetShareEnum(nullptr, 502,
                                    reinterpret_cast<LPBYTE*>(&share_info_),
                                    MAX_PREFERRED_LENGTH,
                                    &entries_read,
                                    &total_entries_,
                                    nullptr);
    if (error_code != NERR_Success)
    {
        DLOG(WARNING) << "NetShareEnum() failed: " << SystemErrorCodeToString(error_code);
        return;
    }

    DCHECK(entries_read == total_entries_);
}

ShareEnumerator::~ShareEnumerator()
{
    if (share_info_)
        NetApiBufferFree(share_info_);
}

bool ShareEnumerator::IsAtEnd() const
{
    if (!share_info_)
        return true;

    return current_entry_ >= total_entries_;
}

void ShareEnumerator::Advance()
{
    ++current_entry_;
}

std::string ShareEnumerator::GetName() const
{
    return UTF8fromUNICODE(share_info_[current_entry_].shi502_netname);
}

ShareEnumerator::Type ShareEnumerator::GetType() const
{
    switch (share_info_[current_entry_].shi502_type)
    {
        case STYPE_DISKTREE:
            return Type::DISK;

        case STYPE_PRINTQ:
            return Type::PRINTER;

        case STYPE_DEVICE:
            return Type::DEVICE;

        case STYPE_IPC:
            return Type::IPC;

        case STYPE_SPECIAL:
            return Type::SPECIAL;

        case STYPE_TEMPORARY:
            return Type::TEMPORARY;

        default:
            return Type::UNKNOWN;
    }
}

std::string ShareEnumerator::GetDescription() const
{
    return UTF8fromUNICODE(share_info_[current_entry_].shi502_remark);
}

std::string ShareEnumerator::GetLocalPath() const
{
    return UTF8fromUNICODE(share_info_[current_entry_].shi502_path);
}

uint32_t ShareEnumerator::GetCurrentUses() const
{
    return share_info_[current_entry_].shi502_current_uses;
}

uint32_t ShareEnumerator::GetMaximumUses() const
{
    return share_info_[current_entry_].shi502_max_uses;
}

} // namespace aspia
