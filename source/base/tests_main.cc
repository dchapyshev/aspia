//
// Aspia Project
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

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/crypto/scoped_crypto_initializer.h"

#include <QCoreApplication>
#include <QTimer>

#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    QCoreApplication::setEventDispatcher(new base::AsioEventDispatcher());
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);

    base::ScopedCryptoInitializer crypto_initializer;
    if (!crypto_initializer.isSucceeded())
        return 1;

    QTimer::singleShot(0, []()
    {
        int result = RUN_ALL_TESTS();
        QCoreApplication::exit(result);
    });

    return app.exec();
}
