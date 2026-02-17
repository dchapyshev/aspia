//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#if defined(Q_OS_UNIX)
#include <dirent.h>
#include <unistd.h>
#include <string>
#endif

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif

#include <gtest/gtest.h>

namespace {

int countOpenHandles()
{
#if defined(Q_OS_WINDOWS)
    DWORD handleCount = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handleCount))
        return static_cast<int>(handleCount);
    return -1;
#else
#if defined(Q_OS_LINUX)
    const char* fd_dir = "/proc/self/fd";
#elif defined(Q_OS_MACOS)
    const char* fd_dir = "/dev/fd";
#else
    return -1;
#endif

    DIR* dir = opendir(fd_dir);
    if (!dir)
        return -1;

    int count = 0;
    while (dirent* entry = readdir(dir))
    {
        if (entry->d_name[0] != '.')
            ++count;
    }
    closedir(dir);
    return count;
#endif
}

} // namespace

int main(int argc, char **argv)
{
    int handles_before = countOpenHandles();
    int ret;

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

        ret = app.exec();
    }

    int handles_after = countOpenHandles();
    GTEST_LOG_(INFO) << "Open handles before: " << handles_before << " after: " << handles_after;

    return ret;
}
