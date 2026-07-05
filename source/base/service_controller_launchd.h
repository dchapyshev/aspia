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

#ifndef BASE_SERVICE_CONTROLLER_LAUNCHD_H
#define BASE_SERVICE_CONTROLLER_LAUNCHD_H

#include "base/service_controller.h"

class ServiceControllerLaunchd final : public ServiceController
{
public:
    ServiceControllerLaunchd();
    ~ServiceControllerLaunchd() final;

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
    bool setAccount(const QString& username, const QString& password,
                    const QStringList& paths) final;
    QString filePath() const final;
    bool isRunning() const final;
    bool start() final;
    bool stop() final;

private:
    explicit ServiceControllerLaunchd(const QString& label);

    // launchd job label, e.g. "org.aspia.host-service". Also the basename of the plist in
    // /Library/LaunchDaemons.
    QString label_;

    Q_DISABLE_COPY_MOVE(ServiceControllerLaunchd)
};

#endif // BASE_SERVICE_CONTROLLER_LAUNCHD_H
