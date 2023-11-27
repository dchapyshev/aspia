//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

namespace common {

// FileEnumerator::FileInfo ----------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
FileEnumerator::FileInfo::FileInfo() = default;

//--------------------------------------------------------------------------------------------------
bool FileEnumerator::FileInfo::isDirectory() const
{
    std::error_code ignored_error;
    return it_->is_directory(ignored_error);
}

//--------------------------------------------------------------------------------------------------
std::filesystem::path FileEnumerator::FileInfo::name() const
{
    return it_->path().filename();
}

//--------------------------------------------------------------------------------------------------
std::string FileEnumerator::FileInfo::u8name() const
{
    return it_->path().filename();
}

//--------------------------------------------------------------------------------------------------
int64_t FileEnumerator::FileInfo::size() const
{
    std::error_code ignored_error;
    return it_->file_size(ignored_error);
}

//--------------------------------------------------------------------------------------------------
time_t FileEnumerator::FileInfo::lastWriteTime() const
{
    auto current_system_time = std::chrono::system_clock::now();
    auto current_file_time = std::filesystem::file_time_type::clock::now();

    std::error_code ignored_error;
    auto file_time = it_->last_write_time(ignored_error);

    auto timepoint = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - current_file_time + current_system_time);

    return std::chrono::system_clock::to_time_t(timepoint);
}

// FileEnumerator --------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
FileEnumerator::FileEnumerator(const std::filesystem::path& root_path)
{
    std::error_code ignored_code;
    file_info_.it_ = std::filesystem::directory_iterator(root_path, ignored_code);
}

//--------------------------------------------------------------------------------------------------
FileEnumerator::~FileEnumerator() = default;

//--------------------------------------------------------------------------------------------------
bool FileEnumerator::isAtEnd() const
{
    return file_info_.it_ == std::filesystem::directory_iterator();
}

//--------------------------------------------------------------------------------------------------
void FileEnumerator::advance()
{
    ++file_info_.it_;
}

} // namespace common
