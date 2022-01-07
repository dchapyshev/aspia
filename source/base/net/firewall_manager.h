//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__NET__FIREWALL_MANAGER_H
#define BASE__NET__FIREWALL_MANAGER_H

#include "base/macros_magic.h"
#include "base/memory/scalable_vector.h"

#include <filesystem>

#include <wrl/client.h>
#include <netfw.h>

namespace base {

class FirewallManager
{
public:
    explicit FirewallManager(const std::filesystem::path& application_path);
    ~FirewallManager() = default;

    // Returns true if firewall manager is valid.
    bool isValid() const;

    // Returns true if firewall is enabled.
    bool isFirewallEnabled() const;

    // Returns true if there is any rule for the application.
    bool hasAnyRule();

    // Adds a firewall rule allowing inbound connections to the application on
    // TCP port |port|. Replaces the rule if it already exists. Needs elevation.
    bool addTcpRule(std::wstring_view rule_name,
                    std::wstring_view description,
                    uint16_t port);

    // Deletes all rules with specified name. Needs elevation.
    void deleteRuleByName(std::wstring_view rule_name);

    // Deletes all rules for current app. Needs elevation.
    void deleteAllRules();

private:
    // Returns the list of rules applying to the application.
    void allRules(ScalableVector<Microsoft::WRL::ComPtr<INetFwRule>>* rules);

    // Deletes rules. Needs elevation.
    void deleteRule(Microsoft::WRL::ComPtr<INetFwRule> rule);

    Microsoft::WRL::ComPtr<INetFwPolicy2> firewall_policy_;
    Microsoft::WRL::ComPtr<INetFwRules> firewall_rules_;

    std::filesystem::path application_path_;

    DISALLOW_COPY_AND_ASSIGN(FirewallManager);
};

} // namespace base

#endif // BASE__NET__FIREWALL_MANAGER_H
