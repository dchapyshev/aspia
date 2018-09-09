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

#include "console/console_main.h"

#include <QCommandLineParser>

#include "base/logging.h"
#include "crypto/scoped_crypto_initializer.h"
#include "console/console_window.h"
#include "version.h"

#include <Windows.h>

namespace aspia {

int consoleMain(int argc, char *argv[])
{
    LoggingSettings settings;
    settings.logging_dest = LOG_TO_ALL;

    ScopedLogging logging(settings);

    ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    QApplication application(argc, argv);

    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Console"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Aspia Console"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("The file to open."));
    parser.process(application);

    QStringList arguments = parser.positionalArguments();

    QString file_path;
    if (arguments.size())
        file_path = arguments.front();

    ConsoleWindow window(file_path);
    window.show();
    window.activateWindow();

    return application.exec();
}

} // namespace aspia
