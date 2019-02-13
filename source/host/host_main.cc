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

#include "host/host_main.h"

#include <QApplication>

#include "base/win/process_util.h"
#include "base/base_paths.h"
#include "base/qt_logging.h"
#include "build/version.h"
#include "crypto/scoped_crypto_initializer.h"
#include "host/ui/host_window.h"

namespace {

std::filesystem::path loggingDir()
{
    std::filesystem::path path;

    if (base::win::isProcessElevated())
    {
        if (!base::BasePaths::commonAppData(&path))
            return std::filesystem::path();
    }
    else
    {
        if (!base::BasePaths::userAppData(&path))
            return std::filesystem::path();
    }

    path.append("aspia/logs");
    return path;
}

int runApplication(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Host"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    host::HostWindow window;
    window.show();
    window.activateWindow();

    return application.exec();
}

} // namespace

int hostMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(qt_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);
    Q_INIT_RESOURCE(updater);
    Q_INIT_RESOURCE(updater_translations);

    base::LoggingSettings settings;
    settings.destination = base::LOG_TO_FILE;
    settings.log_dir = loggingDir();

    base::initLogging(settings);
    base::initQtLogging();

    crypto::ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    int result = runApplication(argc, argv);

    base::shutdownLogging();
    return result;
}
