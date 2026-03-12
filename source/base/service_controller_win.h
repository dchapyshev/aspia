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

#ifndef BASE_SERVICE_CONTROLLER_WIN_H
#define BASE_SERVICE_CONTROLLER_WIN_H

#include <QString>

#include <memory>

#include "base/service_controller.h"
#include "base/win/scoped_object.h"

namespace base {

class ServiceControllerWin final : public ServiceController
{
public:
    ServiceControllerWin(SC_HANDLE sc_manager, SC_HANDLE service);
    ~ServiceControllerWin() final;

    static std::unique_ptr<ServiceController> open(const QString& name);
    static std::unique_ptr<ServiceController> install(
        const QString& name, const QString& display_name, const QString& file_path);
    static bool remove(const QString& name);
    static bool isInstalled(const QString& name);
    static bool isRunning(const QString& name);

    // ServiceController implementation.
    bool setDescription(const QString& description) final;
    QString description() const final;
    bool setDependencies(const QStringList& dependencies) final;
    QStringList dependencies() const final;
    bool setAccount(const QString& username, const QString& password) final;
    QString filePath() const final;
    bool isRunning() const final;
    bool start() final;
    bool stop() final;

private:
    ScopedScHandle sc_manager_;
    mutable ScopedScHandle service_;

    Q_DISABLE_COPY_MOVE(ServiceControllerWin)
};

} // namespace base

#endif // BASE_SERVICE_CONTROLLER_WIN_H
