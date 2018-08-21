//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "network/firewall_manager.h"

#include <QDebug>
#include <QUuid>

#include <comutil.h>
#include <unknwn.h>

#include "base/errno_logging.h"

namespace aspia {

FirewallManager::FirewallManager(std::wstring_view application_path)
    : application_path_(application_path)
{
    // Firewall manager does not work with the wrong path separators.
    std::replace(application_path_.begin(), application_path_.end(), L'/', L'\\');

    // Retrieve INetFwPolicy2
    HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&firewall_policy_));
    if (FAILED(hr))
    {
        qWarning() << "CreateInstance failed: " << errnoToString(hr);
        firewall_policy_ = nullptr;
        return;
    }

    hr = firewall_policy_->get_Rules(firewall_rules_.GetAddressOf());
    if (FAILED(hr))
    {
        qWarning() << "get_Rules failed: " << errnoToString(hr);
        firewall_rules_ = nullptr;
    }
}

bool FirewallManager::isValid() const
{
    return firewall_rules_ && firewall_policy_;
}

bool FirewallManager::isFirewallEnabled() const
{
    long profile_types = 0;

    HRESULT hr = firewall_policy_->get_CurrentProfileTypes(&profile_types);
    if (FAILED(hr))
        return false;

    // The most-restrictive active profile takes precedence.
    static const NET_FW_PROFILE_TYPE2 kProfileTypes[] =
    {
        NET_FW_PROFILE2_PUBLIC,
        NET_FW_PROFILE2_PRIVATE,
        NET_FW_PROFILE2_DOMAIN
    };

    for (size_t i = 0; i < _countof(kProfileTypes); ++i)
    {
        if ((profile_types & kProfileTypes[i]) != 0)
        {
            VARIANT_BOOL enabled = VARIANT_TRUE;

            hr = firewall_policy_->get_FirewallEnabled(kProfileTypes[i], &enabled);

            // Assume the firewall is enabled if we can't determine.
            if (FAILED(hr) || enabled != VARIANT_FALSE)
                return true;
        }
    }

    return false;
}

bool FirewallManager::hasAnyRule()
{
    std::vector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
    allRules(&rules);

    return !rules.empty();
}

bool FirewallManager::addTcpRule(std::wstring_view rule_name,
                                 std::wstring_view description,
                                 int port)
{
    // Delete the rule. According MDSN |INetFwRules::Add| should replace rule with same
    // "rule identifier". It's not clear what is "rule identifier", but it can successfully
    // create many rule with same name.
    deleteRuleByName(rule_name);

    Microsoft::WRL::ComPtr<INetFwRule> rule;

    HRESULT hr = CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rule));
    if (FAILED(hr))
    {
        qWarning() << "CoCreateInstance failed: " << errnoToString(hr);
        return false;
    }

    rule->put_Name(_bstr_t(rule_name.data()));
    rule->put_Description(_bstr_t(description.data()));
    rule->put_ApplicationName(_bstr_t(application_path_.data()));
    rule->put_Protocol(NET_FW_IP_PROTOCOL_TCP);
    rule->put_Direction(NET_FW_RULE_DIR_IN);
    rule->put_Enabled(VARIANT_TRUE);
    rule->put_LocalPorts(_bstr_t(std::to_wstring(port).c_str()));
    rule->put_Profiles(NET_FW_PROFILE2_ALL);
    rule->put_Action(NET_FW_ACTION_ALLOW);

    firewall_rules_->Add(rule.Get());
    if (FAILED(hr))
    {
        qWarning() << "Add failed: " << errnoToString(hr);
        return false;
    }

    return true;
}

void FirewallManager::deleteRuleByName(std::wstring_view rule_name)
{
    std::vector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
    allRules(&rules);

    for (const auto& rule : rules)
    {
        _bstr_t bstr_rule_name;

        HRESULT hr = rule->get_Name(bstr_rule_name.GetAddress());
        if (FAILED(hr))
        {
            qWarning() << "get_Name failed: " << errnoToString(hr);
            continue;
        }

        if (!bstr_rule_name)
            continue;

        if (_wcsicmp(rule_name.data(), static_cast<const wchar_t*>(bstr_rule_name)) == 0)
            deleteRule(rule);
    }
}

void FirewallManager::deleteAllRules()
{
    std::vector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
    allRules(&rules);

    for (const auto& rule : rules)
        deleteRule(rule);
}

void FirewallManager::allRules(std::vector<Microsoft::WRL::ComPtr<INetFwRule>>* rules)
{
    Microsoft::WRL::ComPtr<IUnknown> rules_enum_unknown;

    HRESULT hr = firewall_rules_->get__NewEnum(rules_enum_unknown.GetAddressOf());
    if (FAILED(hr))
    {
        qWarning() << "get__NewEnum failed: " << errnoToString(hr);
        return;
    }

    Microsoft::WRL::ComPtr<IEnumVARIANT> rules_enum;

    hr = rules_enum_unknown.CopyTo(rules_enum.GetAddressOf());
    if (FAILED(hr))
    {
        qWarning() << "QueryInterface failed: " << errnoToString(hr);
        return;
    }

    for (;;)
    {
        _variant_t rule_var;
        hr = rules_enum->Next(1, rule_var.GetAddress(), nullptr);
        if (FAILED(hr))
            qWarning() << "Next failed: " << errnoToString(hr);

        if (hr != S_OK)
            break;

        Q_ASSERT(VT_DISPATCH == rule_var.vt);

        if (VT_DISPATCH != rule_var.vt)
            continue;

        Microsoft::WRL::ComPtr<INetFwRule> rule;

        hr = V_DISPATCH(&rule_var)->QueryInterface(IID_PPV_ARGS(rule.GetAddressOf()));
        if (FAILED(hr))
        {
            qWarning() << "QueryInterface failed: " << errnoToString(hr);
            continue;
        }

        _bstr_t bstr_path;
        hr = rule->get_ApplicationName(bstr_path.GetAddress());
        if (FAILED(hr))
        {
            qWarning() << "get_ApplicationName failed: " << errnoToString(hr);
            continue;
        }

        if (!bstr_path)
            continue;

        if (_wcsicmp(application_path_.data(), static_cast<const wchar_t*>(bstr_path)) != 0)
            continue;

        rules->push_back(rule);
    }
}

void FirewallManager::deleteRule(Microsoft::WRL::ComPtr<INetFwRule> rule)
{
    // Rename rule to unique name and delete by unique name. We can't just delete rule by name.
    // Multiple rules with the same name and different app are possible.
    _bstr_t unique_name(qUtf16Printable(QUuid::createUuid().toString()));

    rule->put_Name(unique_name);
    firewall_rules_->Remove(unique_name);
}

} // namespace aspia
