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

#ifndef BASE_FILES_SCOPED_TEMP_FILE_H
#define BASE_FILES_SCOPED_TEMP_FILE_H

#include "base/macros_magic.h"

#include <filesystem>
#include <fstream>

namespace base {

class ScopedTempFile
{
public:
    explicit ScopedTempFile(const std::filesystem::path& file_path);
    ~ScopedTempFile();

    const std::filesystem::path& filePath() const;
    std::fstream& stream();

private:
    std::filesystem::path file_path_;
    std::fstream stream_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTempFile);
};

} // namespace base

#endif // BASE_FILES_SCOPED_TEMP_FILE_H
