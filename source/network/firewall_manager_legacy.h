//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/firewall_manager_legacy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__FIREWALL_MANAGER_LEGACY_H
#define _ASPIA_NETWORK__FIREWALL_MANAGER_LEGACY_H

#include "base/scoped_comptr.h"
#include "base/macros.h"

#include <netfw.h>

namespace aspia {

// Manages firewall rules using Windows Firewall API. The API is
// available on Windows XP with SP2 and later. Applications should use
// |FirewallManager| instead of this class on Windows Vista and later.
// Most methods need elevation.
class FirewallManagerLegacy
{
public:
    FirewallManagerLegacy() = default;
    ~FirewallManagerLegacy() = default;

    // Initializes object to manage application win name |app_name| and path
    // |app_path|.
    bool Init(const std::wstring& app_name, const std::wstring& app_path);

    // Returns true if firewall is enabled.
    bool IsFirewallEnabled() const;

    // Returns true if function can read rule for the current app. Sets |value|
    // true, if rule allows incoming connections, or false otherwise.
    bool GetAllowIncomingConnection(bool& value);

    // Allows or blocks all incoming connection for current app. Needs elevation.
    bool SetAllowIncomingConnection(bool allow);

    // Deletes rule for current app. Needs elevation.
    void DeleteRule();

private:
    // Returns the authorized applications collection for the local firewall
    // policy's current profile or an empty pointer in case of error.
    ScopedComPtr<INetFwAuthorizedApplications> GetAuthorizedApplications();

    // Creates rule for the current application. If |allow| is true, incoming
    // connections are allowed, blocked otherwise.
    ScopedComPtr<INetFwAuthorizedApplication> CreateAuthorization(bool allow);

    ScopedComPtr<INetFwProfile> current_profile_;

    std::wstring app_name_;
    std::wstring app_path_;

    DISALLOW_COPY_AND_ASSIGN(FirewallManagerLegacy);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__FIREWALL_MANAGER_LEGACY_H
