//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__ENVIRONMENT_H
#define BASE__ENVIRONMENT_H

#include "build/build_config.h"

#include <map>
#include <memory>
#include <string>

namespace base {

namespace env_vars {

#if defined(OS_POSIX)
extern const char kHome[];
#endif

} // namespace env_vars

class Environment
{
public:
    virtual ~Environment();

    // Returns the appropriate platform-specific instance.
    static std::unique_ptr<Environment> create();

    // Gets an environment variable's value and stores it in |result|.
    // Returns false if the key is unset.
    virtual bool get(std::string_view variable_name, std::string* result) = 0;

    // Syntactic sugar for GetVar(variable_name, nullptr);
    virtual bool has(std::string_view variable_name);

    // Returns true on success, otherwise returns false. This method should not
    // be called in a multi-threaded process.
    virtual bool set(std::string_view variable_name, const std::string& new_value) = 0;

    // Returns true on success, otherwise returns false. This method should not
    // be called in a multi-threaded process.
    virtual bool unSet(std::string_view variable_name) = 0;
};

} // namespace base

#endif // BASE__ENVIRONMENT_H
