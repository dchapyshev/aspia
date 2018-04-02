//
// PROJECT:         Aspia
// FILE:            network/firewall_manager.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__FIREWALL_MANAGER_H
#define _ASPIA_NETWORK__FIREWALL_MANAGER_H

#include <netfw.h>
#include <string>

#include "base/win/scoped_comptr.h"

namespace aspia {

class FirewallManager
{
public:
    FirewallManager() = default;
    ~FirewallManager() = default;

    // Initializes object to manage application win name |app_name| and path
    // |app_path|.
    bool Init(const wchar_t* app_name, const wchar_t* app_path);

    // Returns true if firewall is enabled.
    bool IsFirewallEnabled() const;

    // Adds a firewall rule allowing inbound connections to the application on
    // TCP port |port|. Replaces the rule if it already exists. Needs elevation.
    bool AddTCPRule(const wchar_t* rule_name,
                    const wchar_t* description,
                    int port);

    // Deletes all rules with specified name. Needs elevation.
    void DeleteRuleByName(const wchar_t* rule_name);

private:
    ScopedComPtr<INetFwPolicy2> firewall_policy_;
    ScopedComPtr<INetFwRules> firewall_rules_;

    std::wstring app_name_;
    std::wstring app_path_;

    Q_DISABLE_COPY(FirewallManager)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__FIREWALL_MANAGER_H
