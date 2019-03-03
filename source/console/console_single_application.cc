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

#include "console/console_single_application.h"
#include "base/qt_logging.h"

namespace console {

namespace {

const char kActivateWindow[] = "activate_window";
const char kOpenFile[] = "open_file:";

} // namespace

SingleApplication::SingleApplication(int& argc, char* argv[])
    : base::SingleApplication(argc, argv)
{
    connect(this, &SingleApplication::messageReceived, [this](const QByteArray& message)
    {
        if (message.startsWith(kActivateWindow))
        {
            emit windowActivated();
        }
        else if (message.startsWith(kOpenFile))
        {
            QString file_path = QString::fromUtf8(message).remove(kOpenFile);
            if (file_path.isEmpty())
                return;

            emit fileOpened(file_path);
        }
        else
        {
            LOG(LS_ERROR) << "Unhandled message";
        }
    });
}

void SingleApplication::activateWindow()
{
    sendMessage(kActivateWindow);
}

void SingleApplication::openFile(const QString& file_path)
{
    QByteArray message(kOpenFile);
    message.append(file_path.toUtf8());
    sendMessage(message);
}

} // namespace console
