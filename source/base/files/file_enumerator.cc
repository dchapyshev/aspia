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

#include "base/files/file_enumerator.h"

#if defined(Q_OS_WINDOWS)
#include "base/logging.h"
#else
#include <QDateTime>
#endif

namespace base {

namespace {

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
time_t fileTimeToUnixTime(const FILETIME& file_time)
{
    ULARGE_INTEGER ull;

    ull.LowPart = file_time.dwLowDateTime;
    ull.HighPart = file_time.dwHighDateTime;

    return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

//--------------------------------------------------------------------------------------------------
bool shouldSkip(std::wstring_view file_name)
{
    return file_name == L"." || file_name == L"..";
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

// FileEnumerator::FileInfo ----------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
FileEnumerator::FileInfo::FileInfo()
{
#if defined(Q_OS_WINDOWS)
    memset(&find_data_, 0, sizeof(find_data_));
#endif
}

//--------------------------------------------------------------------------------------------------
bool FileEnumerator::FileInfo::isDirectory() const
{
#if defined(Q_OS_WINDOWS)
    return (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    return items_[pos_].isDir();
#endif
}

//--------------------------------------------------------------------------------------------------
QString FileEnumerator::FileInfo::name() const
{
#if defined(Q_OS_WINDOWS)
    return QString::fromWCharArray(find_data_.cFileName);
#else
    return items_[pos_].fileName();
#endif
}

//--------------------------------------------------------------------------------------------------
qint64 FileEnumerator::FileInfo::size() const
{
#if defined(Q_OS_WINDOWS)
    ULARGE_INTEGER size;
    size.HighPart = find_data_.nFileSizeHigh;
    size.LowPart = find_data_.nFileSizeLow;
    return static_cast<qint64>(size.QuadPart);
#else
    return items_[pos_].size();
#endif
}

//--------------------------------------------------------------------------------------------------
time_t FileEnumerator::FileInfo::lastWriteTime() const
{
#if defined(Q_OS_WINDOWS)
    return fileTimeToUnixTime(find_data_.ftLastWriteTime);
#else
    return items_[pos_].lastModified().toSecsSinceEpoch();
#endif
}

// FileEnumerator --------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
FileEnumerator::FileEnumerator(const QString& root_path)
{
#if defined(Q_OS_WINDOWS)
    QString filter = root_path + "/*";

    // Start a new find operation.
    find_handle_ = FindFirstFileExW(qUtf16Printable(filter),
                                    FindExInfoBasic, // Omit short name.
                                    &file_info_.find_data_,
                                    FindExSearchNameMatch,
                                    nullptr,
                                    FIND_FIRST_EX_LARGE_FETCH);
    if (find_handle_ == INVALID_HANDLE_VALUE)
    {
        DWORD error_code = GetLastError();
        LOG(ERROR) << "Unable to get file list for" << root_path << ":"
                   << base::SystemError::toString(error_code);
    }

    while (find_handle_ != INVALID_HANDLE_VALUE)
    {
        if (!shouldSkip(file_info_.find_data_.cFileName))
            break;

        if (!FindNextFileW(find_handle_, &file_info_.find_data_))
        {
            FindClose(find_handle_);
            find_handle_ = INVALID_HANDLE_VALUE;
        }
    }
#else
    QDir dir(root_path);
    file_info_.items_ = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
#endif
}

//--------------------------------------------------------------------------------------------------
FileEnumerator::~FileEnumerator()
{
#if defined(Q_OS_WINDOWS)
    if (find_handle_ != INVALID_HANDLE_VALUE)
        FindClose(find_handle_);
#endif
}

//--------------------------------------------------------------------------------------------------
bool FileEnumerator::isAtEnd() const
{
#if defined(Q_OS_WINDOWS)
    return find_handle_ == INVALID_HANDLE_VALUE;
#else
    return file_info_.pos_ >= file_info_.items_.size();
#endif
}

//--------------------------------------------------------------------------------------------------
void FileEnumerator::advance()
{
#if defined(Q_OS_WINDOWS)
    while (find_handle_ != INVALID_HANDLE_VALUE)
    {
        // Search for the next file/directory.
        if (!FindNextFileW(find_handle_, &file_info_.find_data_))
        {
            FindClose(find_handle_);
            find_handle_ = INVALID_HANDLE_VALUE;
        }
        else
        {
            if (!shouldSkip(file_info_.find_data_.cFileName))
                break;
        }
    }
#else
    ++file_info_.pos_;
#endif
}

} // namespace common
