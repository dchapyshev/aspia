//
// SmartCafe Project
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

#include <comdef.h>
#include <comutil.h>
#include <Unknwn.h>

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
FirewallManager::FirewallManager(const QString& application_path)
    : application_path_(QDir::toNativeSeparators(application_path))
{
    // Retrieve INetFwPolicy2
    _com_error error = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                        IID_PPV_ARGS(&firewall_policy_));
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateInstance failed:" << error;
        firewall_policy_ = nullptr;
        return;
    }

    error = firewall_policy_->get_Rules(firewall_rules_.GetAddressOf());
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "get_Rules failed:" << error;
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

    _com_error error = firewall_policy_->get_CurrentProfileTypes(&profile_types);
    if (FAILED(error.Error()))
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

            error = firewall_policy_->get_FirewallEnabled(kProfileTypes[i], &enabled);

            // Assume the firewall is enabled if we can't determine.
            if (FAILED(error.Error()) || enabled != VARIANT_FALSE)
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

    _com_error error = CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rule));
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CoCreateInstance failed:" << error;
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

    error = firewall_rules_->Add(rule.Get());
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Add failed:" << error;
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

    _com_error error = CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rule));
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CoCreateInstance failed:" << error;
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

    error = firewall_rules_->Add(rule.Get());
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Add failed:" << error;
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

        _com_error error = rule->get_Name(bstr_rule_name.GetAddress());
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "get_Name failed:" << error;
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

    _com_error error = firewall_rules_->get__NewEnum(rules_enum_unknown.GetAddressOf());
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "get__NewEnum failed:" << error;
        return;
    }

    Microsoft::WRL::ComPtr<IEnumVARIANT> rules_enum;

    error = rules_enum_unknown.CopyTo(rules_enum.GetAddressOf());
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "QueryInterface failed:" << error;
        return;
    }

    for (;;)
    {
        _variant_t rule_var;
        error = rules_enum->Next(1, rule_var.GetAddress(), nullptr);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "Next failed:" << error;
        }

        if (error.Error() != S_OK)
            break;

        DCHECK_EQ(VT_DISPATCH, rule_var.vt);

        if (VT_DISPATCH != rule_var.vt)
            continue;

        Microsoft::WRL::ComPtr<INetFwRule> rule;

        error = V_DISPATCH(&rule_var)->QueryInterface(IID_PPV_ARGS(rule.GetAddressOf()));
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "QueryInterface failed:" << error;
            continue;
        }

        _bstr_t bstr_path;
        error = rule->get_ApplicationName(bstr_path.GetAddress());
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "get_ApplicationName failed:" << error;
            continue;
        }

        if (!bstr_path)
            continue;

        if (application_path_.compare(QString::fromWCharArray(bstr_path), Qt::CaseInsensitive) != 0)
            continue;

        rules->emplace_back(rule);
    }
}

//--------------------------------------------------------------------------------------------------
void FirewallManager::deleteRule(Microsoft::WRL::ComPtr<INetFwRule> rule)
{
    // Rename rule to unique name and delete by unique name. We can't just delete rule by name.
    // Multiple rules with the same name and different app are possible.
    _bstr_t unique_name(qUtf16Printable(QUuid::createUuid().toString()));

    rule->put_Name(unique_name);
    firewall_rules_->Remove(unique_name);
}

} // namespace base
