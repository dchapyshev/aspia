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

#include "base/win/net_share_enumerator.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
NetShareEnumerator::NetShareEnumerator()
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
        LOG(ERROR) << "NetShareEnum failed:" << SystemError(error_code).toString();
        total_entries_ = 0;
    }
}

//--------------------------------------------------------------------------------------------------
NetShareEnumerator::~NetShareEnumerator()
{
    if (share_info_)
        NetApiBufferFree(share_info_);
}

//--------------------------------------------------------------------------------------------------
bool NetShareEnumerator::isAtEnd() const
{
    return current_pos_ >= total_entries_;
}

//--------------------------------------------------------------------------------------------------
void NetShareEnumerator::advance()
{
    ++current_pos_;
}

//--------------------------------------------------------------------------------------------------
QString NetShareEnumerator::name() const
{
    if (!share_info_[current_pos_].shi502_netname)
        return QString();

    return QString::fromWCharArray(share_info_[current_pos_].shi502_netname);
}

//--------------------------------------------------------------------------------------------------
QString NetShareEnumerator::localPath() const
{
    if (!share_info_[current_pos_].shi502_path)
        return QString();

    return QString::fromWCharArray(share_info_[current_pos_].shi502_path);
}

//--------------------------------------------------------------------------------------------------
QString NetShareEnumerator::description() const
{
    if (!share_info_[current_pos_].shi502_remark)
        return QString();

    return QString::fromWCharArray(share_info_[current_pos_].shi502_remark);
}

//--------------------------------------------------------------------------------------------------
NetShareEnumerator::Type NetShareEnumerator::type() const
{
    switch (share_info_[current_pos_].shi502_type)
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

//--------------------------------------------------------------------------------------------------
quint32 NetShareEnumerator::currentUses() const
{
    return share_info_[current_pos_].shi502_current_uses;
}

//--------------------------------------------------------------------------------------------------
quint32 NetShareEnumerator::maxUses() const
{
    return share_info_[current_pos_].shi502_max_uses;
}

} // namespace base
