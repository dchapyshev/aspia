//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "crypto/scoped_crypto_initializer.h"
#include "host/win/host_service.h"

namespace aspia {

int hostServiceMain(int argc, char *argv[])
{
    base::LoggingSettings settings;
    settings.logging_dest = base::LOG_TO_ALL;

    base::ScopedLogging logging(settings);

    ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    return HostService().exec(argc, argv);
}

} // namespace aspia
