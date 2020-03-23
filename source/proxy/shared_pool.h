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

#ifndef PROXY__SHARED_POOL_H
#define PROXY__SHARED_POOL_H

#include "proxy/session_key.h"

namespace proxy {

using Pool = std::map<uint32_t, SessionKey>;
using SharedPool = std::shared_ptr<Pool>;

} // namespace proxy

#endif // PROXY__SHARED_POOL_H
