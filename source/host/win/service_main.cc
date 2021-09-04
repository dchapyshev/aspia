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

#include "host/win/service_main.h"

#include "base/scoped_logging.h"
#include "base/win/mini_dump_writer.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/win/service.h"

namespace host {

void hostServiceMain()
{
    base::installFailureHandler(L"aspia_host_service");

    base::ScopedLogging scoped_logging;
    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;

    if (!integrityCheck())
    {
        LOG(LS_ERROR) << "Integrity check failed. Application stopped";
    }
    else
    {
        LOG(LS_INFO) << "Integrity check passed successfully";
        Service().exec();
    }
}

} // namespace host
