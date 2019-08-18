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

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "router/win/router_service.h"

#if defined(USE_TBB)
#include <tbb/tbbmalloc_proxy.h>
#endif // defined(USE_TBB)

namespace {

std::filesystem::path loggingDir()
{
    std::filesystem::path path;

    if (!base::BasePaths::commonAppData(&path))
        return std::filesystem::path();

    path.append("aspia/logs");
    return path;
}

} // namespace

int main(int argc, char* argv[])
{
    base::LoggingSettings settings;
    settings.destination = base::LOG_TO_FILE;
    settings.log_dir = loggingDir();

    router::Service().exec();

    base::shutdownLogging();
    return 0;
}
