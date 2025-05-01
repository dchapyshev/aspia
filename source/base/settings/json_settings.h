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

#ifndef BASE_SETTINGS_JSON_SETTINGS_H
#define BASE_SETTINGS_JSON_SETTINGS_H

#include "base/macros_magic.h"
#include "base/settings/settings.h"

#include <filesystem>

namespace base {

class JsonSettings final : public Settings
{
public:
    enum class Scope { USER, SYSTEM };

    explicit JsonSettings(std::string_view file_name);
    JsonSettings(Scope scope,
                 std::string_view application_name,
                 std::string_view file_name);
    ~JsonSettings() final;

    bool isWritable() const;
    void sync();
    bool flush();

    const std::filesystem::path& filePath() const { return path_; }

    static std::filesystem::path filePath(std::string_view file_name);
    static std::filesystem::path filePath(Scope scope,
                                          std::string_view application_name,
                                          std::string_view file_name);

    static bool readFile(const std::filesystem::path& file, Map& map);
    static bool writeFile(const std::filesystem::path& file, const Map& map);

private:
    std::filesystem::path path_;

    DISALLOW_COPY_AND_ASSIGN(JsonSettings);
};

} // namespace base

#endif // BASE_SETTINGS_JSON_SETTINGS_H
