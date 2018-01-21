//
// PROJECT:         Aspia
// FILE:            base/service_manager.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_MANAGER_H
#define _ASPIA_BASE__SERVICE_MANAGER_H

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/scoped_object.h"

namespace aspia {

class ServiceManager
{
public:
    ServiceManager() = default;
    explicit ServiceManager(const std::wstring_view& service_short_name);
    ~ServiceManager();

    // Creates a service in the system and returns a pointer to the instance
    // of the class to manage it.
    static std::unique_ptr<ServiceManager>
    Create(const CommandLine& command_line,
           const std::wstring_view& service_name,
           const std::wstring_view& service_short_name,
           const std::wstring_view& service_description);

    static std::wstring GenerateUniqueServiceId();

    static std::wstring CreateUniqueServiceName(const std::wstring_view& service_name,
                                                const std::wstring_view& service_id);

    static bool IsServiceInstalled(const std::wstring& service_name);

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

#endif // _ASPIA_BASE__SERVICE_MANAGER_H
