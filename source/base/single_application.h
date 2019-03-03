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

#ifndef BASE__SINGLE_APPLICATION_H
#define BASE__SINGLE_APPLICATION_H

#include "base/macros_magic.h"

#include <QApplication>

class QLocalServer;
class QLocalSocket;
class QLockFile;

namespace base {

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
    QLocalSocket* socket_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(SingleApplication);
};

} // namespace base

#endif // BASE__SINGLE_APPLICATION_H
