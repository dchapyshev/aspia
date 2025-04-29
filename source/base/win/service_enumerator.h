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

#ifndef BASE_WIN_SERVICE_ENUMERATOR_H
#define BASE_WIN_SERVICE_ENUMERATOR_H

#include "base/win/scoped_object.h"

#include <cstdint>
#include <memory>

#include <QString>

namespace base::win {

class ServiceEnumerator
{
public:
    enum class Type { SERVICES = 0, DRIVERS = 1 };

    explicit ServiceEnumerator(Type type);
    ~ServiceEnumerator() = default;

    bool isAtEnd() const;
    void advance();

    QString name() const;
    QString displayName() const;
    QString description() const;

    enum class Status
    {
        UNKNOWN          = 0,
        CONTINUE_PENDING = 1,
        PAUSE_PENDING    = 2,
        PAUSED           = 3,
        RUNNING          = 4,
        START_PENDING    = 5,
        STOP_PENDING     = 6,
        STOPPED          = 7
    };

    Status status() const;

    enum class StartupType
    {
        UNKNOWN      = 0,
        AUTO_START   = 1,
        DEMAND_START = 2,
        DISABLED     = 3,
        BOOT_START   = 4,
        SYSTEM_START = 5
    };

    StartupType startupType() const;
    QString binaryPath() const;
    QString startName() const;

private:
    ENUM_SERVICE_STATUS_PROCESS* currentService() const;
    SC_HANDLE currentServiceHandle() const;
    LPQUERY_SERVICE_CONFIG currentServiceConfig() const;

    mutable ScopedScHandle manager_handle_;

    std::unique_ptr<uint8_t[]> services_buffer_;
    DWORD services_count_ = 0;

    mutable ScopedScHandle current_service_handle_;
    mutable std::unique_ptr<uint8_t[]> current_service_config_;
    DWORD current_service_index_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ServiceEnumerator);
};

} // namespace base::win

#endif // BASE_WIN_SERVICE_ENUMERATOR_H
