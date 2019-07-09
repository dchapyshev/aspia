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

#ifndef QT_BASE__SINGLE_APPLICATION_H
#define QT_BASE__SINGLE_APPLICATION_H

#include "base/macros_magic.h"

#include <QApplication>

class QLocalServer;
class QLockFile;

namespace qt_base {

class SingleApplication : public QApplication
{
    Q_OBJECT

public:
    SingleApplication(int& argc, char* argv[]);
    virtual ~SingleApplication();

    bool isRunning();

public slots:
    void sendMessage(const QByteArray& message);

signals:
    void messageReceived(const QByteArray& message);

private slots:
    void onNewConnection();

private:
    QString lock_file_name_;
    QString server_name_;

    QLockFile* lock_file_ = nullptr;
    QLocalServer* server_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(SingleApplication);
};

} // namespace qt_base

#endif // QT_BASE__SINGLE_APPLICATION_H
