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

#include "host/win/host_service_main.h"

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "crypto/scoped_crypto_initializer.h"
#include "host/win/host_service.h"

namespace host {

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

int hostServiceMain(int argc, char *argv[])
{
    base::LoggingSettings settings;
    settings.destination = base::LOG_TO_FILE;
    settings.log_dir = loggingDir();

    base::initLogging(settings);

    crypto::ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    LOG(LS_INFO) << "BEGIN SERVICE";

    Service().exec();

    base::shutdownLogging();
    return 0;
}

} // namespace host
