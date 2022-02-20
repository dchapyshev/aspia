//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_SCOPED_LOGGING_H
#define BASE_SCOPED_LOGGING_H

#include "base/logging.h"

namespace base {

class ScopedLogging
{
public:
    ScopedLogging(const LoggingSettings& settings = LoggingSettings())
    {
        initialized_ = initLogging(settings);
    }

    ~ScopedLogging()
    {
        if (initialized_)
            shutdownLogging();
    }

private:
    bool initialized_ = false;
};

} // namespace base

#endif // BASE_SCOPED_LOGGING_H
