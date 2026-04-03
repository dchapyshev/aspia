//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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
FileStream::FileStream(qint64 file_size)
    : file_size_(file_size),
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
    terminate();
}

//--------------------------------------------------------------------------------------------------
void FileStream::terminate()
{
    {
        std::scoped_lock lock(lock_);
        is_terminated_ = true;
    }

    if (!SetEvent(data_event_))
        PLOG(ERROR) << "SetEvent failed";
}

//--------------------------------------------------------------------------------------------------
void FileStream::addData(const QByteArray& data)
{
    {
        std::scoped_lock lock(lock_);
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
        AddRef();
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
    if (new_ref == 0)
        delete this;
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
    ULONG total_read = 0;

    while (total_read < cb)
    {
        lock_.lock();

        if (!buffer_.isEmpty())
        {
            qsizetype to_copy = std::min(qsizetype(cb - total_read), buffer_.size());
            memcpy(reinterpret_cast<char*>(pv) + total_read, buffer_.data(), to_copy);

            buffer_.remove(0, to_copy);
            total_read += to_copy;

            lock_.unlock();
        }
        else if (is_terminated_)
        {
            lock_.unlock();
            break;
        }
        else
        {
            lock_.unlock();

            // Use MsgWaitForMultipleObjects to pump messages while waiting.
            // This prevents deadlock when Read() is called on the STA thread
            // via OLE marshaling: without message pumping, the thread would block
            // and could not process Qt queued connections that deliver file data.
            HANDLE event = data_event_;

            for (;;)
            {
                DWORD ret = MsgWaitForMultipleObjects(
                    1, &event, FALSE, INFINITE, QS_ALLINPUT);

                if (ret == WAIT_OBJECT_0)
                {
                    break;
                }
                else if (ret == WAIT_OBJECT_0 + 1)
                {
                    MSG msg;
                    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
                else
                {
                    PLOG(ERROR) << "MsgWaitForMultipleObjects failed:" << ret;
                    return S_FALSE;
                }
            }
        }
    }

    if (pcbRead)
        *pcbRead = total_read;

    return total_read > 0 ? S_OK : S_FALSE;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
{
    NOTIMPLEMENTED();
    return STG_E_INVALIDFUNCTION;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
{
    NOTIMPLEMENTED();
    return STG_E_ACCESSDENIED;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::SetSize(ULARGE_INTEGER libNewSize)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::CopyTo(
    IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Commit(DWORD grfCommitFlags)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Revert()
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Stat(STATSTG* pstatstg, DWORD grfStatFlag)
{
    if (!pstatstg)
        return STG_E_INVALIDPOINTER;

    memset(pstatstg, 0, sizeof(STATSTG));
    pstatstg->type = STGTY_STREAM;
    pstatstg->cbSize.QuadPart = static_cast<ULONGLONG>(file_size_);

    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileStream::Clone(IStream** ppstm)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

} // namespace common
