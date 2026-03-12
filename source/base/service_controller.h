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

#ifndef BASE_SERVICE_CONTROLLER_H
#define BASE_SERVICE_CONTROLLER_H

#include <QString>

#include <memory>

namespace base {

class ServiceController
{
public:
    ServiceController() = default;
    virtual ~ServiceController() = default;

    static std::unique_ptr<ServiceController> open(const QString& name);
    static std::unique_ptr<ServiceController> install(
        const QString& name, const QString& display_name, const QString& file_path);
    static bool remove(const QString& name);
    static bool isInstalled(const QString& name);
    static bool isRunning(const QString& name);

    virtual bool setDescription(const QString& description) = 0;
    virtual QString description() const = 0;
    virtual bool setDependencies(const QStringList& dependencies) = 0;
    virtual QStringList dependencies() const = 0;
    virtual bool setAccount(const QString& username, const QString& password) = 0;
    virtual QString filePath() const = 0;
    virtual bool isRunning() const = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;

private:
    Q_DISABLE_COPY_MOVE(ServiceController)
};

} // namespace base

#endif // BASE_SERVICE_CONTROLLER_H
