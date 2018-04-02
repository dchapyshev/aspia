//
// PROJECT:         Aspia
// FILE:            network/firewall_manager.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/firewall_manager.h"

#include <QDebug>

#include <objbase.h>
#include <unknwn.h>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/system_error_code.h"

namespace aspia {

bool FirewallManager::Init(const wchar_t* app_name, const wchar_t* app_path)
{
    firewall_rules_ = nullptr;

    // Retrieve INetFwPolicy2
    HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(firewall_policy_.GetAddressOf()));
    if (FAILED(hr))
    {
        qWarning() << "CreateInstance failed: " << systemErrorCodeToString(hr);
        firewall_policy_ = nullptr;
        return false;
    }

    hr = firewall_policy_->get_Rules(firewall_rules_.GetAddressOf());
    if (FAILED(hr))
    {
        qWarning() << "get_Rules failed: " << systemErrorCodeToString(hr);
        firewall_rules_ = nullptr;
        return false;
    }

    app_path_ = app_path;
    app_name_ = app_name;

    return true;
}

bool FirewallManager::IsFirewallEnabled() const
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

bool FirewallManager::AddTCPRule(const wchar_t* rule_name,
                                 const wchar_t* description,
                                 int port)
{
    DeleteRuleByName(rule_name);

    ScopedComPtr<INetFwRule> rule;

    HRESULT hr = CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(rule.GetAddressOf()));
    if (FAILED(hr))
    {
        qWarning() << "CoCreateInstance failed: " << systemErrorCodeToString(hr);
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

    firewall_rules_->Add(rule.Get());
    if (FAILED(hr))
    {
        qWarning() << "Add failed: " << systemErrorCodeToString(hr);
        return false;
    }

    return true;
}

void FirewallManager::DeleteRuleByName(const wchar_t* rule_name)
{
    ScopedComPtr<IUnknown> rules_enum_unknown;

    HRESULT hr = firewall_rules_->get__NewEnum(rules_enum_unknown.GetAddressOf());
    if (FAILED(hr))
    {
        qWarning() << "get__NewEnum failed: " << systemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<IEnumVARIANT> rules_enum;

    hr = rules_enum_unknown.CopyTo(rules_enum.GetAddressOf());
    if (FAILED(hr))
    {
        qWarning() << "QueryInterface failed: " << systemErrorCodeToString(hr);
        return;
    }

    while (true)
    {
        ScopedVariant rule_var;

        hr = rules_enum->Next(1, rule_var.Receive(), nullptr);

        if (FAILED(hr))
            qWarning() << "rules_enum->Next failed: " << systemErrorCodeToString(hr);

        if (hr != S_OK)
            break;

        Q_ASSERT(VT_DISPATCH == rule_var.type());

        ScopedComPtr<INetFwRule> rule;

        hr = V_DISPATCH(&rule_var)->QueryInterface(IID_PPV_ARGS(rule.GetAddressOf()));
        if (FAILED(hr))
        {
            qWarning() << "QueryInterface failed: " << systemErrorCodeToString(hr);
            continue;
        }

        ScopedBstr bstr_rule_name;

        hr = rule->get_Name(bstr_rule_name.Receive());
        if (FAILED(hr))
        {
            qWarning() << "get_Name failed: " << systemErrorCodeToString(hr);
            continue;
        }

        if (bstr_rule_name && _wcsicmp(bstr_rule_name, rule_name) == 0)
        {
            hr = firewall_rules_->Remove(bstr_rule_name);
            if (FAILED(hr))
            {
                qWarning() << "Remove failed: " << systemErrorCodeToString(hr);
            }
        }
    }
}

} // namespace aspia
