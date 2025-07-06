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
#include <QMutex>

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
    HRESULT __stdcall Seek(LARGE_INTEGER, DWORD, ULARGE_INTEGER*) final;
    HRESULT __stdcall Write(const void*, ULONG, ULONG*) final;
    HRESULT __stdcall SetSize(ULARGE_INTEGER) final;
    HRESULT __stdcall CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) final;
    HRESULT __stdcall Commit(DWORD) final;
    HRESULT __stdcall Revert() final;
    HRESULT __stdcall LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) final;
    HRESULT __stdcall UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) final;
    HRESULT __stdcall Stat(STATSTG* pstatstg, DWORD) final;
    HRESULT __stdcall Clone(IStream**) final;

    LONG ref_count_;

    base::ScopedHandle data_event_;
    QByteArray buffer_;
    bool is_terminated_ = false;
    QMutex lock_;
};

} // namespace common

#endif // COMMON_WIN_FILE_STREAM_H
