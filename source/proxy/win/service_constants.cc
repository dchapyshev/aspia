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

#include "proxy/win/service_constants.h"

namespace proxy {

const char16_t kServiceFileName[] = u"aspia_proxy.exe";

const char16_t kServiceName[] = u"aspia-proxy";

const char16_t kServiceDisplayName[] = u"Aspia Proxy Service";

const char16_t kServiceDescription[] = u"Proxies user traffic to bypass NAT.";

} // namespace proxy
