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

#include "base/environment.h"

#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "build/build_config.h"

#include <memory>

#if defined(OS_WIN)
#include <Windows.h>
#elif defined(OS_POSIX)
#include <stdlib.h>
#endif

namespace base {

namespace {

bool getImpl(std::string_view variable_name, std::string* result)
{
#if defined(OS_WIN)
    DWORD value_length =
        GetEnvironmentVariableW(wideFromUtf8(variable_name).c_str(), nullptr, 0);
    if (value_length == 0)
        return false;

    if (result)
    {
        std::unique_ptr<wchar_t[]> value = std::make_unique<wchar_t[]>(value_length);
        GetEnvironmentVariableW(
            wideFromUtf8(variable_name).c_str(), value.get(), value_length);
        *result = utf8FromWide(value.get());
    }
    return true;
#elif defined(OS_POSIX)
    const char* env_value = getenv(variable_name.data());
    if (!env_value)
        return false;
    // Note that the variable may be defined but empty.
    if (result)
        *result = env_value;
    return true;
#endif
}

bool setImpl(std::string_view variable_name, const std::string& new_value)
{
#if defined(OS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariableW(wideFromUtf8(variable_name).c_str(),
                                     wideFromUtf8(new_value).c_str());
#elif defined(OS_POSIX)
    // On success, zero is returned.
    return !setenv(variable_name.data(), new_value.c_str(), 1);
#endif
}

bool unSetImpl(std::string_view variable_name)
{
#if defined(OS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariableW(wideFromUtf8(variable_name).c_str(), nullptr);
#elif defined(OS_POSIX)
    // On success, zero is returned.
    return !unsetenv(variable_name.data());
#endif
}

}  // namespace

namespace env_vars {

#if defined(OS_POSIX)
// On Posix systems, this variable contains the location of the user's home
// directory. (e.g, /home/username/).
const char kHome[] = "HOME";
#endif

}  // namespace env_vars

// static
bool Environment::get(std::string_view variable_name, std::string* result)
{
    if (getImpl(variable_name, result))
        return true;

    // Some commonly used variable names are uppercase while others are lowercase, which is
    // inconsistent. Let's try to be helpful and look for a variable name with the reverse case.
    // I.e. HTTP_PROXY may be http_proxy for some users/systems.
    char first_char = variable_name[0];
    std::string alternate_case_var;

    if (isLowerASCII(first_char))
        alternate_case_var = toUpperASCII(variable_name);
    else if (isUpperASCII(first_char))
        alternate_case_var = toLowerASCII(variable_name);
    else
        return false;

    return getImpl(alternate_case_var, result);
}

// static
bool Environment::has(std::string_view variable_name)
{
    return get(variable_name, nullptr);
}

// static
bool Environment::set(std::string_view variable_name, const std::string& new_value)
{
    return setImpl(variable_name, new_value);
}

// static
bool Environment::unSet(std::string_view variable_name)
{
    return unSetImpl(variable_name);
}

} // namespace base
