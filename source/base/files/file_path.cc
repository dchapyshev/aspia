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

#include "base/files/file_path.h"

#include "build/build_config.h"

namespace base {

std::string utf8FromFilePath(const std::filesystem::path& path)
{
#if defined(OS_WIN)
    //return base::utf8FromWide(path.c_str());
#else
    //return path.c_str();
#endif
}

std::filesystem::path filePathFromUtf8(std::string_view str)
{
#if defined(OS_WIN)
    //return std::filesystem::path(base::wideFromUtf8(str));
#else
    //return std::filesystem::path(std::string(str));
#endif
}

} // namespace base

