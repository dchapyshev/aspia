//
// SmartCafe Project
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

#include "base/net/curl_util.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedCURL::ScopedCURL()
    : curl_(curl_easy_init())
{
    CHECK(curl_);
}

//--------------------------------------------------------------------------------------------------
ScopedCURL::~ScopedCURL()
{
    curl_easy_cleanup(curl_);
}

//--------------------------------------------------------------------------------------------------
ScopedCURLM::ScopedCURLM()
    : curlm_(curl_multi_init())
{
    CHECK(curlm_);
}

//--------------------------------------------------------------------------------------------------
ScopedCURLM::~ScopedCURLM()
{
    curl_multi_cleanup(curlm_);
}

} // namespace base
