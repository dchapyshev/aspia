//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/firewall_manager.h
// LICENSE:         See top-level directory
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

    bool Init(const std::wstring& app_name, const std::wstring& app_path);
    bool IsFirewallEnabled();
    bool AddTcpRule(const WCHAR* rule_name, const WCHAR* description, uint16_t port);
    void DeleteRule(const WCHAR* rule_name);

private:
    ScopedComPtr<INetFwPolicy2> firewall_policy_;
    ScopedComPtr<INetFwRules> firewall_rules_;

    std::wstring app_name_;
    std::wstring app_path_;

    DISALLOW_COPY_AND_ASSIGN(FirewallManager);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__FIREWALL_MANAGER_H
