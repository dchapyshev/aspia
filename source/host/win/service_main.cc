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

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "crypto/scoped_crypto_initializer.h"
#include "host/win/service.h"

namespace host {

namespace {

void initLogging()
{
    std::filesystem::path path;

    if (!base::BasePaths::commonAppData(&path))
        return;

    path.append("aspia/logs");

    base::LoggingSettings settings;
    settings.destination = base::LOG_TO_FILE;
    settings.log_dir = path;

    base::initLogging(settings);
}

void shutdownLogging()
{
    base::shutdownLogging();
}

} // namespace

void hostServiceMain()
{
    initLogging();

    crypto::ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    Service().exec();

    shutdownLogging();
}

} // namespace host
