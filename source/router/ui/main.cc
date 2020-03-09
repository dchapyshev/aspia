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

#include "router/ui/main_window.h"

#include <QApplication>

#if defined(QT_STATIC)

#include <QtPlugin>

#if defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin);
#else
#error Platform support needed
#endif // defined(Q_OS_WIN)
#endif // defined(QT_STATIC)

#if defined(USE_TBB)
#include <tbb/tbbmalloc_proxy.h>
#endif // defined(USE_TBB)

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(qt_translations);

    QApplication application(argc, argv);

    router::MainWindow main_window;
    main_window.show();
    main_window.activateWindow();

    return QApplication::exec();
}
