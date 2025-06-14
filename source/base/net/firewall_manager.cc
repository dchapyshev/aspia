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

#include "base/net/firewall_manager.h"

#include <QDir>
#include <QUuid>

#include "base/logging.h"

#include <comutil.h>
#include <Unknwn.h>

namespace base {

//--------------------------------------------------------------------------------------------------
FirewallManager::FirewallManager(const QString& application_path)
    : application_path_(QDir::toNativeSeparators(application_path))
{
    // Retrieve INetFwPolicy2
    HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&firewall_policy_));
    if (FAILED(hr))
    {
        LOG(ERROR) << "CreateInstance failed:" << SystemError::toString(static_cast<DWORD>(hr));
        firewall_policy_ = nullptr;
        return;
    }

    hr = firewall_policy_->get_Rules(firewall_rules_.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(ERROR) << "get_Rules failed:" << SystemError::toString(static_cast<DWORD>(hr));
        firewall_rules_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
bool FirewallManager::isValid() const
{
    return firewall_rules_ && firewall_policy_;
}

//--------------------------------------------------------------------------------------------------
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

    for (size_t i = 0; i < std::size(kProfileTypes); ++i)
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

//--------------------------------------------------------------------------------------------------
bool FirewallManager::hasAnyRule()
{
    QVector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
    allRules(&rules);

    return !rules.empty();
}

//--------------------------------------------------------------------------------------------------
bool FirewallManager::addTcpRule(const QString& rule_name,
                                 const QString& description,
                                 quint16 port)
{
    // Delete the rule. According MDSN |INetFwRules::Add| should replace rule with same
    // "rule identifier". It's not clear what is "rule identifier", but it can successfully
    // create many rule with same name.
    deleteRuleByName(rule_name);

    Microsoft::WRL::ComPtr<INetFwRule> rule;

    HRESULT hr = CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rule));
    if (FAILED(hr))
    {
        LOG(ERROR) << "CoCreateInstance failed:" << SystemError::toString(static_cast<DWORD>(hr));
        return false;
    }

    rule->put_Name(_bstr_t(qUtf16Printable(rule_name)));
    rule->put_Description(_bstr_t(qUtf16Printable(description)));
    rule->put_ApplicationName(_bstr_t(qUtf16Printable(application_path_)));
    rule->put_Protocol(NET_FW_IP_PROTOCOL_TCP);
    rule->put_Direction(NET_FW_RULE_DIR_IN);
    rule->put_Enabled(VARIANT_TRUE);
    rule->put_LocalPorts(_bstr_t(std::to_wstring(port).c_str()));
    rule->put_Profiles(NET_FW_PROFILE2_ALL);
    rule->put_Action(NET_FW_ACTION_ALLOW);

    firewall_rules_->Add(rule.Get());
    if (FAILED(hr))
    {
        LOG(ERROR) << "Add failed:" << SystemError::toString(static_cast<DWORD>(hr));
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool FirewallManager::addUdpRule(const QString& rule_name,
                                 const QString& description,
                                 quint16 port)
{
    // Delete the rule. According MDSN |INetFwRules::Add| should replace rule with same
    // "rule identifier". It's not clear what is "rule identifier", but it can successfully
    // create many rule with same name.
    deleteRuleByName(rule_name);

    Microsoft::WRL::ComPtr<INetFwRule> rule;

    HRESULT hr = CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rule));
    if (FAILED(hr))
    {
        LOG(ERROR) << "CoCreateInstance failed:" << SystemError::toString(static_cast<DWORD>(hr));
        return false;
    }

    rule->put_Name(_bstr_t(qUtf16Printable(rule_name)));
    rule->put_Description(_bstr_t(qUtf16Printable(description)));
    rule->put_ApplicationName(_bstr_t(qUtf16Printable(application_path_)));
    rule->put_Protocol(NET_FW_IP_PROTOCOL_UDP);
    rule->put_Direction(NET_FW_RULE_DIR_IN);
    rule->put_Enabled(VARIANT_TRUE);
    rule->put_LocalPorts(_bstr_t(std::to_wstring(port).c_str()));
    rule->put_Profiles(NET_FW_PROFILE2_ALL);
    rule->put_Action(NET_FW_ACTION_ALLOW);

    firewall_rules_->Add(rule.Get());
    if (FAILED(hr))
    {
        LOG(ERROR) << "Add failed:" << SystemError::toString(static_cast<DWORD>(hr));
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void FirewallManager::deleteRuleByName(const QString& rule_name)
{
    QVector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
    allRules(&rules);

    for (const auto& rule : std::as_const(rules))
    {
        _bstr_t bstr_rule_name;

        HRESULT hr = rule->get_Name(bstr_rule_name.GetAddress());
        if (FAILED(hr))
        {
            LOG(ERROR) << "get_Name failed:" << SystemError::toString(static_cast<DWORD>(hr));
            continue;
        }

        if (!bstr_rule_name)
            continue;

        if (rule_name.compare(QString::fromWCharArray(bstr_rule_name), Qt::CaseInsensitive) == 0)
            deleteRule(rule);
    }
}

//--------------------------------------------------------------------------------------------------
void FirewallManager::deleteAllRules()
{
    QVector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
    allRules(&rules);

    for (const auto& rule : std::as_const(rules))
        deleteRule(rule);
}

//--------------------------------------------------------------------------------------------------
void FirewallManager::allRules(QVector<Microsoft::WRL::ComPtr<INetFwRule>>* rules)
{
    Microsoft::WRL::ComPtr<IUnknown> rules_enum_unknown;

    HRESULT hr = firewall_rules_->get__NewEnum(rules_enum_unknown.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(ERROR) << "get__NewEnum failed:" << SystemError::toString(static_cast<DWORD>(hr));
        return;
    }

    Microsoft::WRL::ComPtr<IEnumVARIANT> rules_enum;

    hr = rules_enum_unknown.CopyTo(rules_enum.GetAddressOf());
    if (FAILED(hr))
    {
        LOG(ERROR) << "QueryInterface failed:" << SystemError::toString(static_cast<DWORD>(hr));
        return;
    }

    for (;;)
    {
        _variant_t rule_var;
        hr = rules_enum->Next(1, rule_var.GetAddress(), nullptr);
        if (FAILED(hr))
        {
            LOG(ERROR) << "Next failed:" << SystemError::toString(static_cast<DWORD>(hr));
        }

        if (hr != S_OK)
            break;

        DCHECK_EQ(VT_DISPATCH, rule_var.vt);

        if (VT_DISPATCH != rule_var.vt)
            continue;

        Microsoft::WRL::ComPtr<INetFwRule> rule;

        hr = V_DISPATCH(&rule_var)->QueryInterface(IID_PPV_ARGS(rule.GetAddressOf()));
        if (FAILED(hr))
        {
            LOG(ERROR) << "QueryInterface failed:" << SystemError::toString(static_cast<DWORD>(hr));
            continue;
        }

        _bstr_t bstr_path;
        hr = rule->get_ApplicationName(bstr_path.GetAddress());
        if (FAILED(hr))
        {
            LOG(ERROR) << "get_ApplicationName failed:" << SystemError::toString(static_cast<DWORD>(hr));
            continue;
        }

        if (!bstr_path)
            continue;

        if (application_path_.compare(QString::fromWCharArray(bstr_path), Qt::CaseInsensitive) != 0)
            continue;

        rules->push_back(rule);
    }
}

//--------------------------------------------------------------------------------------------------
void FirewallManager::deleteRule(Microsoft::WRL::ComPtr<INetFwRule> rule)
{
    // Rename rule to unique name and delete by unique name. We can't just delete rule by name.
    // Multiple rules with the same name and different app are possible.
    _bstr_t unique_name(QUuid::createUuid().toString().toStdWString().c_str());

    rule->put_Name(unique_name);
    firewall_rules_->Remove(unique_name);
}

} // namespace base
