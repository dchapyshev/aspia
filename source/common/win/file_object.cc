//
// SmartCafe Project
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

#include "common/win/file_object.h"

#include <strsafe.h>

#include "base/logging.h"
#include "base/win/scoped_hglobal.h"
#include "common/win/format_enumerator.h"

namespace common {

namespace {

//--------------------------------------------------------------------------------------------------
bool unixTimeToFileTime(qint64 unix_time, FILETIME* file_time)
{
    if (unix_time < 0 || !file_time)
        return false;

    const ULONGLONG epoch_diff = 116444736000000000ULL;
    ULONGLONG filetime_value = static_cast<ULONGLONG>(unix_time) * 10000000ULL + epoch_diff;

    file_time->dwLowDateTime = static_cast<DWORD>(filetime_value & 0xFFFFFFFF);
    file_time->dwHighDateTime = static_cast<DWORD>(filetime_value >> 32);
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
FileObject::FileObject(const proto::desktop::ClipboardEvent::FileList& files)
    : files_(files)
{
    file_group_descriptor_ = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
    if (!file_group_descriptor_)
    {
        PLOG(ERROR) << "RegisterClipboardFormat failed";
        return;
    }

    file_contents_ = RegisterClipboardFormat(CFSTR_FILECONTENTS);
    if (!file_contents_)
    {
        PLOG(ERROR) << "RegisterClipboardFormat failed";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
FileObject::~FileObject()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
FileObject* FileObject::create(const proto::desktop::ClipboardEvent::FileList& files)
{
    std::unique_ptr<FileObject> object(new FileObject(files));

    if (!object->file_group_descriptor_ || !object->file_contents_)
    {
        LOG(ERROR) << "Format is not registered";
        return nullptr;
    }

    return object.release();
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::QueryInterface(REFIID iid, void** object)
{
    if (object == nullptr)
        return E_POINTER;

    if (iid == IID_IDataObject || iid == IID_IUnknown)
    {
        *object = static_cast<IDataObject*>(this);
        return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
}

//--------------------------------------------------------------------------------------------------
ULONG FileObject::AddRef()
{
    ULONG new_ref = InterlockedIncrement(&ref_count_);
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
ULONG FileObject::Release()
{
    ULONG new_ref = InterlockedDecrement(&ref_count_);
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::GetData(FORMATETC* pFormatEtc, STGMEDIUM* pMedium)
{
    if (pFormatEtc->cfFormat == file_group_descriptor_)
    {
        const quint64 item_count = static_cast<size_t>(files_.file_size());
        CHECK(item_count > 0);

        FILEGROUPDESCRIPTORW file_group_descriptor;
        memset(&file_group_descriptor, 0, sizeof(FILEGROUPDESCRIPTORW));
        file_group_descriptor.cItems = item_count;

        std::unique_ptr<FILEDESCRIPTORW[]> file_descriptor =
            std::make_unique<FILEDESCRIPTORW[]>(item_count);
        memset(file_descriptor.get(), 0, sizeof(FILEDESCRIPTORW) * item_count);

        for (quint64 i = 0; i < item_count; ++i)
        {
            const proto::desktop::ClipboardEvent::FileList::File& file = files_.file(static_cast<int>(i));
            const QString file_path = QString::fromStdString(file.path());
            FILEDESCRIPTORW* current = &file_descriptor[i];

            StringCchCopyW(current->cFileName, std::size(current->cFileName), qUtf16Printable(file_path));

            unixTimeToFileTime(file.create_time(), &current->ftCreationTime);
            unixTimeToFileTime(file.access_time(), &current->ftLastAccessTime);
            unixTimeToFileTime(file.modify_time(), &current->ftLastWriteTime);

            current->dwFlags = FD_ATTRIBUTES | FD_CREATETIME | FD_ACCESSTIME | FD_WRITESTIME;

            if (file.is_dir())
            {
                current->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            }
            else
            {
                current->dwFlags |= FD_FILESIZE | FD_PROGRESSUI;
                current->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

                ULARGE_INTEGER size;
                size.QuadPart = static_cast<quint64>(file.file_size());

                current->nFileSizeLow = size.LowPart;
                current->nFileSizeHigh = size.HighPart;
            }
        }

        SIZE_T total_size = sizeof(UINT) + (sizeof(FILEDESCRIPTORW) * item_count);

        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, total_size);
        if (!memory)
        {
            PLOG(ERROR) << "GlobalAlloc failed";
            return E_OUTOFMEMORY;
        }

        {
            base::ScopedHGLOBAL<BYTE> data(memory);

            memcpy(data.get(), &file_group_descriptor, sizeof(UINT));
            memcpy(data.get() + sizeof(UINT), file_descriptor.get(), sizeof(FILEDESCRIPTORW) * item_count);
        }

        pMedium->tymed = TYMED_HGLOBAL;
        pMedium->hGlobal = memory;
        pMedium->pUnkForRelease = nullptr;

        return S_OK;
    }
    else if (pFormatEtc->cfFormat == file_contents_)
    {
        const int index = pFormatEtc->lindex;
        const int count = files_.file_size();

        if (index < 0 && index >= count)
        {
            LOG(ERROR) << "Invalid lindex:" << index << "(count:" << count << ")";
            return DV_E_LINDEX;
        }

        const proto::desktop::ClipboardEvent::FileList::File& file = files_.file(index);
        if (file.is_dir())
        {
            LOG(ERROR) << "Directory index passed:" << index;
            return DV_E_LINDEX;
        }

        LOG(INFO) << "File to copy:" << QString::fromStdString(file.path());

        // TODO

        stream_ = std::make_unique<FileStream>();

        pMedium->tymed = TYMED_ISTREAM;
        pMedium->pUnkForRelease = nullptr;
        pMedium->pstm = stream_.get();

        return S_OK;
    }

    return DV_E_FORMATETC;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::GetDataHere(FORMATETC*, STGMEDIUM*)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::QueryGetData(FORMATETC* pFormatEtc)
{
    if (pFormatEtc->cfFormat == file_group_descriptor_ || pFormatEtc->cfFormat == file_contents_)
        return S_OK;

    return DV_E_FORMATETC;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::GetCanonicalFormatEtc(FORMATETC*, FORMATETC*)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::SetData(FORMATETC*, STGMEDIUM*, BOOL)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppEnum)
{
    if (dwDirection != DATADIR_GET)
    {
        NOTIMPLEMENTED();
        return E_NOTIMPL;
    }

    QVector<FORMATETC> formats;

    FORMATETC file_group_descriptor_format =
    {
        static_cast<CLIPFORMAT>(file_group_descriptor_),
        nullptr,
        DVASPECT_CONTENT,
        -1,
        TYMED_HGLOBAL
    };

    FORMATETC file_contents_format =
    {
        static_cast<CLIPFORMAT>(file_contents_),
        nullptr,
        DVASPECT_CONTENT,
        -1,
        TYMED_ISTREAM
    };

    formats.emplace_back(file_group_descriptor_format);
    formats.emplace_back(file_contents_format);

    *ppEnum = new FormatEnumerator(formats);
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::DUnadvise(DWORD)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------------------
HRESULT FileObject::EnumDAdvise(IEnumSTATDATA**)
{
    NOTIMPLEMENTED();
    return E_NOTIMPL;
}

} // namespace common
