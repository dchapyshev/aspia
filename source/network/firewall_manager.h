//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/firewall_manager.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__FIREWALL_MANAGER_H
#define _ASPIA_NETWORK__FIREWALL_MANAGER_H

#include "base/scoped_comptr.h"
#include "base/macros.h"

#include <netfw.h>
#include <string>

namespace aspia {

class FirewallManager
{
public:
    FirewallManager() = default;
    ~FirewallManager() = default;

    // Initializes object to manage application win name |app_name| and path
    // |app_path|.
    bool Init(const std::wstring& app_name, const std::wstring& app_path);

    // Returns true if firewall is enabled.
    bool IsFirewallEnabled();

    // Adds a firewall rule allowing inbound connections to the application on TCP
    // port |port|. Replaces the rule if it already exists. Needs elevation.
    bool AddTCPRule(const std::wstring& rule_name,
                    const std::wstring& description,
                    uint16_t port);

    // Deletes all rules with specified name. Needs elevation.
    void DeleteRuleByName(const std::wstring& rule_name);

private:
    ScopedComPtr<INetFwPolicy2> firewall_policy_;
    ScopedComPtr<INetFwRules> firewall_rules_;

    std::wstring app_name_;
    std::wstring app_path_;

    DISALLOW_COPY_AND_ASSIGN(FirewallManager);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__FIREWALL_MANAGER_H
