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

#ifndef BASE_WIN_SERVICE_CONTROLLER_H
#define BASE_WIN_SERVICE_CONTROLLER_H

#include "base/win/scoped_object.h"

#include <QString>

namespace base {

class ServiceController
{
public:
    ServiceController();

    ServiceController(ServiceController&& other) noexcept;
    ServiceController& operator=(ServiceController&& other) noexcept;

    virtual ~ServiceController();

    static ServiceController open(const QString& name);
    static ServiceController install(const QString& name,
                                     const QString& display_name,
                                     const QString& file_path);
    static bool remove(const QString& name);
    static bool isInstalled(const QString& name);
    static bool isRunning(const QString& name);

    void close();

    bool setDescription(const QString& description);
    QString description() const;

    bool setDependencies(const QStringList& dependencies);
    QStringList dependencies() const;

    bool setAccount(const QString& username, const QString& password);

    QString filePath() const;

    bool isValid() const;
    bool isRunning() const;

    bool start();
    bool stop();

protected:
    ServiceController(SC_HANDLE sc_manager, SC_HANDLE service);

private:
    ScopedScHandle sc_manager_;
    mutable ScopedScHandle service_;

    DISALLOW_COPY_AND_ASSIGN(ServiceController);
};

} // namespace base

#endif // BASE_WIN_SERVICE_CONTROLLER_H
