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

#include "common/clipboard_win.h"

#include <QStack>

#include <comdef.h>
#include <ole2.h>
#include <shellapi.h>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/win/message_window.h"
#include "base/win/scoped_clipboard.h"
#include "base/win/scoped_hglobal.h"
#include "base/win/scoped_object.h"
#include "common/win/file_object.h"

namespace common {

namespace {

//--------------------------------------------------------------------------------------------------
struct FileInfo
{
    bool is_directory = false;
    qint64 file_size = 0;
    qint64 create_time = 0;
    qint64 access_time = 0;
    qint64 modify_time = 0;
};

//--------------------------------------------------------------------------------------------------
bool isDirectory(const QString& path)
{
    DWORD attributes = GetFileAttributesW(qUtf16Printable(path));
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return false;

    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

//--------------------------------------------------------------------------------------------------
qint64 fileTimeToUnixTime(const FILETIME& file_time)
{
    ULARGE_INTEGER ull;

    ull.LowPart = file_time.dwLowDateTime;
    ull.HighPart = file_time.dwHighDateTime;

    return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

//--------------------------------------------------------------------------------------------------
bool fileInfo(const QString& path, FileInfo* file_info)
{
    if (path.isEmpty() || !file_info)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    file_info->is_directory = isDirectory(path);

    const DWORD flags = file_info->is_directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;

    base::ScopedHandle file(CreateFileW(qUtf16Printable(path), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, flags, nullptr));
    if (!file.isValid())
    {
        PLOG(WARNING) << "Unable to open file" << path;
        return false;
    }

    if (!file_info->is_directory)
    {
        LARGE_INTEGER size;
        if (!GetFileSizeEx(file, &size))
        {
            LOG(WARNING) << "Unable to get file size" << path;
            return false;
        }

        file_info->file_size = size.QuadPart;
    }

    FILETIME filetime_create;
    FILETIME filetime_access;
    FILETIME filetime_write;

    if (!GetFileTime(file, &filetime_create, &filetime_access, &filetime_write))
    {
        PLOG(WARNING) << "Unable to get file time" << path;
        return false;
    }

    file_info->create_time = fileTimeToUnixTime(filetime_create);
    file_info->access_time = fileTimeToUnixTime(filetime_access);
    file_info->modify_time = fileTimeToUnixTime(filetime_write);
    return true;
}

//--------------------------------------------------------------------------------------------------
void addDirectoryContent(const QString& path, proto::desktop::ClipboardEvent::FileList* files)
{
    QStack<QString> stack;
    stack.push(path);

    while (!stack.isEmpty())
    {
        QString current_path = stack.top();
        stack.pop();

        QString search_path = current_path + QLatin1String("\\*");

        WIN32_FIND_DATAW find_data;
        HANDLE find_handle = FindFirstFileExW(qUtf16Printable(search_path),
                                              FindExInfoBasic, // Omit short name.
                                              &find_data,
                                              FindExSearchNameMatch,
                                              nullptr,
                                              FIND_FIRST_EX_LARGE_FETCH);
        if (find_handle == INVALID_HANDLE_VALUE)
            continue;

        do
        {
            QString name = QString::fromWCharArray(find_data.cFileName);

            if (name == QLatin1String(".") || name == QLatin1String(".."))
                continue;

            QString full_path = current_path + QLatin1Char('\\') + name;
            bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            proto::desktop::ClipboardEvent::FileList::File* file = files->add_file();

            file->set_path(full_path.toStdString());
            file->set_is_dir(is_directory);
            file->set_create_time(fileTimeToUnixTime(find_data.ftCreationTime));
            file->set_access_time(fileTimeToUnixTime(find_data.ftLastAccessTime));
            file->set_modify_time(fileTimeToUnixTime(find_data.ftLastWriteTime));

            if (is_directory)
            {
                stack.push(full_path);
            }
            else
            {
                ULARGE_INTEGER size;
                size.HighPart = find_data.nFileSizeHigh;
                size.LowPart = find_data.nFileSizeLow;

                file->set_file_size(static_cast<qint64>(size.QuadPart));
            }

        }
        while (FindNextFileW(find_handle, &find_data));

        FindClose(find_handle);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClipboardWin::ClipboardWin(QObject* parent)
    : Clipboard(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClipboardWin::~ClipboardWin()
{
    LOG(INFO) << "Dtor";

    if (!window_)
    {
        LOG(ERROR) << "Window not created";
        return;
    }

    RemoveClipboardFormatListener(window_->hwnd());
    window_.reset();
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::init()
{
    if (window_)
    {
        LOG(ERROR) << "Window already created";
        return;
    }

    window_ = std::make_unique<base::MessageWindow>();

    if (!window_->create(std::bind(&ClipboardWin::onMessage,
                                   this,
                                   std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3, std::placeholders::_4)))
    {
        LOG(ERROR) << "Couldn't create clipboard window";
        return;
    }

    if (!AddClipboardFormatListener(window_->hwnd()))
    {
        PLOG(ERROR) << "AddClipboardFormatListener failed";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::setData(const QString& mime_type, const QByteArray& data)
{
    if (!window_)
    {
        LOG(ERROR) << "Window not created";
        return;
    }

    if (mime_type == kMimeTypeTextUtf8)
    {
        setDataText(data);
    }
    else if (mime_type == kMimeTypeFileList)
    {
        setDataFiles(data);
    }
    else
    {
        LOG(WARNING) << "Unhandled mime type:" << mime_type;
    }
}

//--------------------------------------------------------------------------------------------------
bool ClipboardWin::onMessage(UINT message, WPARAM /* wParam */, LPARAM /* lParam */, LRESULT& result)
{
    switch (message)
    {
        case WM_CLIPBOARDUPDATE:
            onClipboardUpdate();
            break;

        default:
            return false;
    }

    result = 0;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::onClipboardUpdate()
{
    if (IsClipboardFormatAvailable(CF_HDROP))
    {
        onClipboardFiles();
        return;
    }

    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        onClipboardText();
        return;
    }

    // Unhandled clipboard type.
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::onClipboardText()
{
    QString data;

    // Add a scope, so that we keep the clipboard open for as short a time as possible.
    {
        base::ScopedClipboard clipboard;

        if (!clipboard.init(window_->hwnd()))
        {
            PLOG(ERROR) << "Couldn't open the clipboard";
            return;
        }

        HGLOBAL text_global = clipboard.data(CF_UNICODETEXT);
        if (!text_global)
        {
            PLOG(ERROR) << "Couldn't get data from the clipboard";
            return;
        }

        {
            base::ScopedHGLOBAL<wchar_t> text_lock(text_global);
            if (!text_lock.get())
            {
                PLOG(ERROR) << "Couldn't lock clipboard data";
                return;
            }

            data = QString::fromWCharArray(text_lock.get());
        }
    }

    if (!data.isEmpty())
    {
        data.replace("\r\n", "\n");
        onData(kMimeTypeTextUtf8, data.toUtf8());
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::onClipboardFiles()
{
    QStringList files;

    // Add a scope, so that we keep the clipboard open for as short a time as possible.
    {
        base::ScopedClipboard clipboard;

        if (!clipboard.init(window_->hwnd()))
        {
            PLOG(ERROR) << "Couldn't open the clipboard";
            return;
        }

        HDROP drop_handle = reinterpret_cast<HDROP>(clipboard.data(CF_HDROP));
        if (!drop_handle)
        {
            PLOG(ERROR) << "Couldn't get data from the clipboard";
            return;
        }

        UINT file_count = DragQueryFileW(drop_handle, 0xFFFFFFFF, nullptr, 0);

        for (UINT i = 0; i < file_count; ++i)
        {
            UINT length = DragQueryFileW(drop_handle, i, nullptr, 0); // Length without \0.
            if (!length)
            {
                PLOG(ERROR) << "DragQueryFileW failed";
                continue;
            }

            std::wstring file_path(length + 1, L'\0');

            if (!DragQueryFileW(drop_handle, i, file_path.data(), length + 1))
            {
                PLOG(ERROR) << "DragQueryFileW failed";
                continue;
            }

            files.append(QString::fromWCharArray(file_path.c_str(), wcslen(file_path.c_str())));
        }
    }

    proto::desktop::ClipboardEvent::FileList file_list;

    for (const auto& path : std::as_const(files))
    {
        FileInfo file_info;
        if (!fileInfo(path, &file_info))
            continue;

        proto::desktop::ClipboardEvent::FileList::File* file = file_list.add_file();
        file->set_path(path.toStdString());
        file->set_is_dir(file_info.is_directory);
        file->set_file_size(file_info.file_size);
        file->set_create_time(file_info.create_time);
        file->set_access_time(file_info.access_time);
        file->set_modify_time(file_info.modify_time);

        // When copying, only top-level paths are copied to the clipboard. If the directory has
        // nested elements, they must be added separately.
        if (file_info.is_directory)
            addDirectoryContent(path, &file_list);
    }

    onData(kMimeTypeFileList, base::serialize(file_list));
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::setDataText(const QByteArray& data)
{
    QString text = QString::fromUtf8(data);
    text.replace("\n", "\r\n");

    base::ScopedClipboard clipboard;
    if (!clipboard.init(window_->hwnd()))
    {
        PLOG(ERROR) << "Couldn't open the clipboard";
        return;
    }

    clipboard.empty();

    if (text.isEmpty())
        return;

    HGLOBAL text_global = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (!text_global)
    {
        PLOG(ERROR) << "GlobalAlloc failed";
        return;
    }

    LPWSTR text_global_locked = reinterpret_cast<LPWSTR>(GlobalLock(text_global));
    if (!text_global_locked)
    {
        PLOG(ERROR) << "GlobalLock failed";
        GlobalFree(text_global);
        return;
    }

    memcpy(text_global_locked, text.utf16(), text.size() * sizeof(wchar_t));
    text_global_locked[text.size()] = 0;

    GlobalUnlock(text_global);

    clipboard.setData(CF_UNICODETEXT, text_global);
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::setDataFiles(const QByteArray& data)
{
    proto::desktop::ClipboardEvent::FileList files;

    if (!base::parse(data, &files))
    {
        LOG(ERROR) << "Unable to parse file list";
        return;
    }

    if (!files.file_size())
    {
        LOG(ERROR) << "Empty file list";
        return;
    }

    _com_error error = OleInitialize(nullptr);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "OleInitialize failed:" << error;
        return;
    }

    do
    {
        file_object_.reset(FileObject::create(files));
        if (!file_object_)
        {
            LOG(ERROR) << "Invalid file object";
            break;
        }

        error = OleSetClipboard(file_object_.get());
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "OleSetClipboard failed:" << error;
            break;
        }
    }
    while (false);

    OleUninitialize();
}

} // namespace common
