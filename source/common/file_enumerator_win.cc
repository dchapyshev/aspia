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

#include "common/file_enumerator.h"

#include "base/logging.h"
#include "base/strings/unicode.h"

namespace common {

namespace {

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

} // namespace

// FileEnumerator::FileInfo ----------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
FileEnumerator::FileInfo::FileInfo()
{
    memset(&find_data_, 0, sizeof(find_data_));
}

//--------------------------------------------------------------------------------------------------
bool FileEnumerator::FileInfo::isDirectory() const
{
    return (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

//--------------------------------------------------------------------------------------------------
std::filesystem::path FileEnumerator::FileInfo::name() const
{
    return std::filesystem::path(find_data_.cFileName);
}

//--------------------------------------------------------------------------------------------------
std::string FileEnumerator::FileInfo::u8name() const
{
    return base::utf8FromWide(find_data_.cFileName);
}

//--------------------------------------------------------------------------------------------------
qint64 FileEnumerator::FileInfo::size() const
{
    ULARGE_INTEGER size;
    size.HighPart = find_data_.nFileSizeHigh;
    size.LowPart = find_data_.nFileSizeLow;
    return static_cast<qint64>(size.QuadPart);
}

//--------------------------------------------------------------------------------------------------
time_t FileEnumerator::FileInfo::lastWriteTime() const
{
    return fileTimeToUnixTime(find_data_.ftLastWriteTime);
}

// FileEnumerator --------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
FileEnumerator::FileEnumerator(const std::filesystem::path& root_path)
{
    std::filesystem::path filter(root_path);
    filter.append(L"*");

    // Start a new find operation.
    find_handle_ = FindFirstFileExW(filter.c_str(),
                                    FindExInfoBasic, // Omit short name.
                                    &file_info_.find_data_,
                                    FindExSearchNameMatch,
                                    nullptr,
                                    FIND_FIRST_EX_LARGE_FETCH);
    if (find_handle_ == INVALID_HANDLE_VALUE)
    {
        DWORD error_code = GetLastError();

        switch (error_code)
        {
            case ERROR_ACCESS_DENIED:
                error_code_ = proto::FILE_ERROR_ACCESS_DENIED;
                break;

            case ERROR_NOT_READY:
                error_code_ = proto::FILE_ERROR_DISK_NOT_READY;
                break;

            default:
                LOG(LS_ERROR) << "Unhandled error code: "
                              << base::SystemError::toString(error_code);
                break;
        }
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
}

//--------------------------------------------------------------------------------------------------
FileEnumerator::~FileEnumerator()
{
    if (find_handle_ != INVALID_HANDLE_VALUE)
        FindClose(find_handle_);
}

//--------------------------------------------------------------------------------------------------
bool FileEnumerator::isAtEnd() const
{
    return find_handle_ == INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------------------------------
void FileEnumerator::advance()
{
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
}

} // namespace common
