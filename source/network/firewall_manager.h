//
// PROJECT:         Aspia
// FILE:            network/firewall_manager.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__FIREWALL_MANAGER_H
#define _ASPIA_NETWORK__FIREWALL_MANAGER_H

#include <QString>

#include <wrl/client.h>
#include <netfw.h>

namespace aspia {

class FirewallManager
{
public:
    explicit FirewallManager(const QString& application_path);
    ~FirewallManager() = default;

    // Returns true if firewall manager is valid.
    bool isValid() const;

    // Returns true if firewall is enabled.
    bool isFirewallEnabled() const;

    // Returns true if there is any rule for the application.
    bool hasAnyRule();

    // Adds a firewall rule allowing inbound connections to the application on
    // TCP port |port|. Replaces the rule if it already exists. Needs elevation.
    bool addTCPRule(const QString& rule_name,
                    const QString& description,
                    int port);

    // Deletes all rules with specified name. Needs elevation.
    void deleteRuleByName(const QString& rule_name);

    // Deletes all rules for current app. Needs elevation.
    void deleteAllRules();

private:
    // Returns the list of rules applying to the application.
    void allRules(QVector<Microsoft::WRL::ComPtr<INetFwRule>>* rules);

    // Deletes rules. Needs elevation.
    void deleteRule(Microsoft::WRL::ComPtr<INetFwRule> rule);

    Microsoft::WRL::ComPtr<INetFwPolicy2> firewall_policy_;
    Microsoft::WRL::ComPtr<INetFwRules> firewall_rules_;

    QString application_path_;

    Q_DISABLE_COPY(FirewallManager)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__FIREWALL_MANAGER_H
