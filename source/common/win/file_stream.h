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

#ifndef COMMON_WIN_FILE_STREAM_H
#define COMMON_WIN_FILE_STREAM_H

#include <QByteArray>
#include <mutex>

#include <shlobj.h>

#include "base/win/scoped_object.h"

namespace common {

class FileStream final : public IStream
{
public:
    FileStream();
    virtual ~FileStream() final;

    void addData(const QByteArray& data);

private:
    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID iid, void** object) final;
    ULONG __stdcall AddRef() final;
    ULONG __stdcall Release() final;

    // IStream
    HRESULT __stdcall Read(void* pv, ULONG cb, ULONG* pcbRead) final;
    HRESULT __stdcall Seek(
        LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) final;
    HRESULT __stdcall Write(const void* pv, ULONG cb, ULONG* pcbWritten) final;
    HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize) final;
    HRESULT __stdcall CopyTo(
        IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) final;
    HRESULT __stdcall Commit(DWORD grfCommitFlags) final;
    HRESULT __stdcall Revert() final;
    HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) final;
    HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) final;
    HRESULT __stdcall Stat(STATSTG* pstatstg, DWORD grfStatFlag) final;
    HRESULT __stdcall Clone(IStream** ppstm) final;

    LONG ref_count_ = 1;

    base::ScopedHandle data_event_;
    QByteArray buffer_;
    bool is_terminated_ = false;
    std::mutex lock_;
};

} // namespace common

#endif // COMMON_WIN_FILE_STREAM_H
