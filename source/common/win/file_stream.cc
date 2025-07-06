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

#include "common/win/file_stream.h"

#include "base/logging.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FileStream::FileStream()
    : ref_count_(1),
      data_event_(CreateEventW(nullptr, FALSE, FALSE, nullptr))
{
    if (!data_event_.isValid())
    {
        PLOG(ERROR) << "CreateEvent failed";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
FileStream::~FileStream()
{
    {
        QMutexLocker<QMutex> lock(&lock_);
        is_terminated_ = true;
    }

    if (!SetEvent(data_event_))
        PLOG(ERROR) << "SetEvent failed";
}

//--------------------------------------------------------------------------------------------------
void FileStream::addData(const QByteArray& data)
{
    {
        QMutexLocker<QMutex> lock(&lock_);
        buffer_.append(data);
    }

    if (!SetEvent(data_event_))
        PLOG(ERROR) << "SetEvent failed";
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::QueryInterface(REFIID iid, void** object)
{
    if (object == nullptr)
        return E_POINTER;

    if (iid == IID_IUnknown || iid == IID_IStream)
    {
        *object = static_cast<IStream*>(this);
        return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
}

//--------------------------------------------------------------------------------------------------
ULONG FileStream::AddRef()
{
    ULONG new_ref = InterlockedIncrement(&ref_count_);
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
ULONG FileStream::Release()
{
    ULONG new_ref = InterlockedDecrement(&ref_count_);
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
    ULONG total_read = 0;

    while (total_read < cb)
    {
        lock_.lock();

        if (is_terminated_)
        {
            lock_.unlock();
            return S_FALSE;
        }
        else if (!buffer_.isEmpty())
        {
            qint64 to_copy = std::min(static_cast<qint64>(cb - total_read), buffer_.size());
            memcpy(reinterpret_cast<char*>(pv) + total_read, buffer_.data(), to_copy);

            buffer_.remove(0, to_copy);
            total_read += to_copy;

            lock_.unlock();
        }
        else
        {
            lock_.unlock();

            DWORD ret = WaitForSingleObject(data_event_, INFINITE);
            if (ret != WAIT_OBJECT_0)
            {
                PLOG(ERROR) << "WaitForSingleObject failed:" << ret;
                return S_FALSE;
            }
        }
    }

    if (pcbRead)
        *pcbRead = total_read;

    return total_read > 0 ? S_OK : S_FALSE;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Seek(LARGE_INTEGER, DWORD, ULARGE_INTEGER*)
{
    return STG_E_INVALIDFUNCTION;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Write(const void*, ULONG, ULONG*)
{
    return STG_E_ACCESSDENIED;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::SetSize(ULARGE_INTEGER)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Commit(DWORD)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Revert()
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
{
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Stat(STATSTG* pstatstg, DWORD)
{
    if (!pstatstg)
        return STG_E_INVALIDPOINTER;

    memset(pstatstg, 0, sizeof(STATSTG));
    pstatstg->type = STGTY_STREAM;

    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Clone(IStream**)
{
    return E_NOTIMPL;
}

} // namespace common
