//
// PROJECT:         Aspia
// FILE:            base/win/service_manager.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SERVICE_MANAGER_H
#define _ASPIA_BASE__WIN__SERVICE_MANAGER_H

#include <memory>
#include <string>

#include "base/win/scoped_object.h"
#include "base/command_line.h"
#include "base/macros.h"

namespace aspia {

class ServiceManager
{
public:
    ServiceManager() = default;
    explicit ServiceManager(const wchar_t* service_short_name);
    ~ServiceManager();

    // Creates a service in the system and returns a pointer to the instance
    // of the class to manage it.
    static std::unique_ptr<ServiceManager>
    Create(const CommandLine& command_line,
           const wchar_t* service_name,
           const wchar_t* service_short_name,
           const wchar_t* service_description);

    enum ServiceStatus
    {
        SERVICE_STATUS_INSTALLED = 1,
        SERVICE_STATUS_STARTED = 2
    };

    static uint32_t GetServiceStatus(const wchar_t* service_name);

    // Starts the service.
    // If the service is successfully started, it returns true, if not,
    // then false.
    bool Start() const;

    // Stops the service.
    // If the service is successfully stopped, it returns true, if not,
    // then false.
    bool Stop() const;

    // Removes the service from the system.
    // After calling the method, the class becomes invalid, calls to other
    // methods will return with an error and the IsValid() method returns
    // false.
    bool Remove();

private:
    ServiceManager(SC_HANDLE sc_manager, SC_HANDLE service);

    mutable ScopedScHandle sc_manager_;
    mutable ScopedScHandle service_;

    DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SERVICE_MANAGER_H
