//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/firewall_manager.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/firewall_manager.h"
#include "base/scoped_bstr.h"
#include "base/scoped_variant.h"
#include "base/logging.h"

#include <objbase.h>
#include <unknwn.h>

namespace aspia {

bool FirewallManager::Init(const std::wstring& app_name, const std::wstring& app_path)
{
    firewall_rules_ = nullptr;

    // Retrieve INetFwPolicy2
    HRESULT hr = firewall_policy_.CreateInstance(CLSID_NetFwPolicy2);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CreateInstance() failed: " << SystemErrorCodeToString(hr);
        firewall_policy_ = nullptr;
        return false;
    }

    hr = firewall_policy_->get_Rules(firewall_rules_.Receive());
    if (FAILED(hr))
    {
        LOG(ERROR) << "get_Rules() failed: " << SystemErrorCodeToString(hr);
        firewall_rules_ = nullptr;
        return false;
    }

    app_path_ = app_path;
    app_name_ = app_name;

    return true;
}

bool FirewallManager::IsFirewallEnabled()
{
    long profile_types = 0;

    HRESULT hr = firewall_policy_->get_CurrentProfileTypes(&profile_types);
    if (FAILED(hr))
        return false;

    // The most-restrictive active profile takes precedence.
    const NET_FW_PROFILE_TYPE2 kProfileTypes[] =
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

bool FirewallManager::AddTCPRule(const std::wstring& rule_name,
                                 const std::wstring& description,
                                 uint16_t port)
{
    DeleteRuleByName(rule_name);

    ScopedComPtr<INetFwRule> rule;

    HRESULT hr = rule.CreateInstance(CLSID_NetFwRule);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CoCreateInstance() failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    rule->put_Name(ScopedBstr(rule_name.c_str()));
    rule->put_Description(ScopedBstr(description.c_str()));
    rule->put_ApplicationName(ScopedBstr(app_path_.c_str()));
    rule->put_Protocol(NET_FW_IP_PROTOCOL_TCP);
    rule->put_Direction(NET_FW_RULE_DIR_IN);
    rule->put_Enabled(VARIANT_TRUE);
    rule->put_LocalPorts(ScopedBstr(std::to_wstring(port).c_str()));
    rule->put_Grouping(ScopedBstr(app_name_.c_str()));
    rule->put_Profiles(NET_FW_PROFILE2_ALL);
    rule->put_Action(NET_FW_ACTION_ALLOW);

    firewall_rules_->Add(rule.get());
    if (FAILED(hr))
    {
        LOG(ERROR) << "Add() failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    return true;
}

void FirewallManager::DeleteRuleByName(const std::wstring& rule_name)
{
    ScopedComPtr<IUnknown> rules_enum_unknown;

    HRESULT hr = firewall_rules_->get__NewEnum(rules_enum_unknown.Receive());
    if (FAILED(hr))
    {
        LOG(ERROR) << "get__NewEnum() failed: " << SystemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<IEnumVARIANT> rules_enum;

    hr = rules_enum.QueryFrom(rules_enum_unknown.get());
    if (FAILED(hr))
    {
        LOG(ERROR) << "QueryInterface() failed: " << SystemErrorCodeToString(hr);
        return;
    }

    while (true)
    {
        ScopedVariant rule_var;

        hr = rules_enum->Next(1, rule_var.Receive(), nullptr);

        DLOG_IF(ERROR, FAILED(hr)) << SystemErrorCodeToString(hr);

        if (hr != S_OK)
            break;

        DCHECK_EQ(VT_DISPATCH, rule_var.type());

        ScopedComPtr<INetFwRule> rule;

        hr = rule.QueryFrom(V_DISPATCH(&rule_var));
        if (FAILED(hr))
        {
            DLOG(ERROR) << "QueryInterface() failed: " << SystemErrorCodeToString(hr);
            continue;
        }

        ScopedBstr bstr_rule_name;

        hr = rule->get_Name(bstr_rule_name.Receive());
        if (FAILED(hr))
        {
            DLOG(ERROR) << "get_Name() failed: " << SystemErrorCodeToString(hr);
            continue;
        }

        if (bstr_rule_name && _wcsicmp(bstr_rule_name, rule_name.c_str()) == 0)
        {
            hr = firewall_rules_->Remove(bstr_rule_name);
            if (FAILED(hr))
            {
                DLOG(ERROR) << "Remove() failed: " << SystemErrorCodeToString(hr);
                continue;
            }
        }
    }
}

} // namespace aspia
