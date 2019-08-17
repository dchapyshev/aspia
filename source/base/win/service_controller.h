//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__WIN__SERVICE_CONTROLLER_H
#define BASE__WIN__SERVICE_CONTROLLER_H

#include "base/win/scoped_object.h"

#include <filesystem>
#include <string>
#include <vector>

namespace base::win {

class ServiceController
{
public:
    ServiceController();

    ServiceController(ServiceController&& other) noexcept;
    ServiceController& operator=(ServiceController&& other) noexcept;

    virtual ~ServiceController();

    static ServiceController open(std::u16string_view name);
    static ServiceController install(std::u16string_view name,
                                     std::u16string_view display_name,
                                     const std::filesystem::path& file_path);
    static bool remove(std::u16string_view name);
    static bool isInstalled(std::u16string_view name);
    static bool isRunning(std::u16string_view name);

    void close();

    bool setDescription(std::u16string_view description);
    std::u16string description() const;

    bool setDependencies(const std::vector<std::u16string>& dependencies);
    std::vector<std::u16string> dependencies() const;

    std::filesystem::path filePath() const;

    bool isValid() const;
    bool isRunning() const;

    bool start();
    bool stop();

protected:
    ServiceController(SC_HANDLE sc_manager, SC_HANDLE service);

private:
    ScopedScHandle sc_manager_;
    mutable ScopedScHandle service_;

    DISALLOW_COPY_AND_ASSIGN(ServiceController);
};

} // namespace base::win

#endif // BASE__WIN__SERVICE_CONTROLLER_H
