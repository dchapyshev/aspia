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

#ifndef COMMON_WIN_FILE_OBJECT_H
#define COMMON_WIN_FILE_OBJECT_H

#include <memory>
#include <shlobj.h>

#include "common/win/file_stream.h"
#include "proto/desktop.h"

namespace common {

class FileObject final : public IDataObject
{
public:
    explicit FileObject(const proto::desktop::ClipboardEvent::FileList& files);
    virtual ~FileObject() final;

    bool isValid() const;

private:
    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID iid, void** object) final;
    ULONG __stdcall AddRef() final;
    ULONG __stdcall Release() final;

    // IDataObject
    HRESULT __stdcall GetData(FORMATETC* pFormatEtc, STGMEDIUM* pMedium) final;
    HRESULT __stdcall GetDataHere(FORMATETC*, STGMEDIUM*) final;
    HRESULT __stdcall QueryGetData(FORMATETC*) final;
    HRESULT __stdcall GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) final;
    HRESULT __stdcall SetData(FORMATETC*, STGMEDIUM*, BOOL) final;
    HRESULT __stdcall EnumFormatEtc(DWORD, IEnumFORMATETC**) final;
    HRESULT __stdcall DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) final;
    HRESULT __stdcall DUnadvise(DWORD) final;
    HRESULT __stdcall EnumDAdvise(IEnumSTATDATA**) final;

    LONG ref_count_ = 1;
    UINT file_group_descriptor_ = 0;
    UINT file_contents_ = 0;

    proto::desktop::ClipboardEvent::FileList files_;
    std::unique_ptr<FileStream> stream_;
};

} // namespace common

#endif // COMMON_WIN_FILE_OBJECT_H
