//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/firewall_manager.cc
// LICENSE:         See top-level directory
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
    // Retrieve INetFwPolicy2
    HRESULT hr = firewall_policy_.CreateInstance(CLSID_NetFwPolicy2);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CreateInstance() failed: " << hr;
        return false;
    }

    hr = firewall_policy_->get_Rules(firewall_rules_.Receive());
    if (FAILED(hr))
    {
        LOG(ERROR) << "get_Rules() failed: " << hr;
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

    for (size_t i = 0; i < ARRAYSIZE(kProfileTypes); ++i)
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

bool FirewallManager::AddTcpRule(const WCHAR* rule_name, const WCHAR* description, uint16_t port)
{
    DeleteRule(rule_name);

    ScopedComPtr<INetFwRule> rule;

    HRESULT hr = rule.CreateInstance(CLSID_NetFwRule);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CoCreateInstance() failed: " << hr;
        return false;
    }

    rule->put_Name(ScopedBstr(rule_name));
    rule->put_Description(ScopedBstr(description));
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
        LOG(ERROR) << "Add() failed: " << hr;
        return false;
    }

    return true;
}

void FirewallManager::DeleteRule(const WCHAR* rule_name)
{
    ScopedComPtr<IUnknown> rules_enum_unknown;

    HRESULT hr = firewall_rules_->get__NewEnum(rules_enum_unknown.Receive());
    if (FAILED(hr))
    {
        LOG(ERROR) << "get__NewEnum() failed: " << hr;
        return;
    }

    ScopedComPtr<IEnumVARIANT> rules_enum;

    hr = rules_enum.QueryFrom(rules_enum_unknown.get());
    if (FAILED(hr))
    {
        LOG(ERROR) << "QueryInterface() failed: " << hr;
        return;
    }

    while (true)
    {
        ScopedVariant rule_var;

        hr = rules_enum->Next(1, rule_var.Receive(), nullptr);

        DLOG_IF(ERROR, FAILED(hr)) << hr;

        if (hr != S_OK)
            break;

        DCHECK_EQ(VT_DISPATCH, rule_var.type());

        ScopedComPtr<INetFwRule> rule;

        hr = rule.QueryFrom(V_DISPATCH(&rule_var));
        if (FAILED(hr))
        {
            DLOG(ERROR) << "QueryInterface() failed: " << hr;
            continue;
        }

        ScopedBstr bstr_rule_name;

        hr = rule->get_Name(bstr_rule_name.Receive());
        if (FAILED(hr))
        {
            DLOG(ERROR) << "get_Name() failed: " << hr;
            continue;
        }

        if (bstr_rule_name && _wcsicmp(bstr_rule_name, rule_name) == 0)
        {
            hr = firewall_rules_->Remove(bstr_rule_name);
            if (FAILED(hr))
            {
                DLOG(ERROR) << "Remove() failed: " << hr;
                continue;
            }
        }
    }
}

} // namespace aspia
