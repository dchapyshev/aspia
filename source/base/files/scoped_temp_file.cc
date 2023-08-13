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

#include "base/files/scoped_temp_file.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedTempFile::ScopedTempFile(const std::filesystem::path& file_path)
    : file_path_(file_path)
{
    const std::ios_base::openmode mode =
        std::fstream::binary | std::fstream::in | std::fstream::out | std::fstream::trunc;

    stream_.open(file_path, mode);
}

//--------------------------------------------------------------------------------------------------
ScopedTempFile::~ScopedTempFile()
{
    stream_.close();

    std::error_code ignored_code;
    std::filesystem::remove(file_path_, ignored_code);
}

//--------------------------------------------------------------------------------------------------
const std::filesystem::path& ScopedTempFile::filePath() const
{
    return file_path_;
}

//--------------------------------------------------------------------------------------------------
std::fstream& ScopedTempFile::stream()
{
    return stream_;
}

} // namespace base
