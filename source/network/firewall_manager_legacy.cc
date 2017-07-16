//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/firewall_manager_legacy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/firewall_manager_legacy.h"
#include "base/scoped_bstr.h"
#include "base/logging.h"

namespace aspia {

bool FirewallManagerLegacy::Init(const std::wstring& app_name, const std::wstring& app_path)
{
    ScopedComPtr<INetFwMgr> firewall_manager;

    HRESULT hr = firewall_manager.CreateInstance(CLSID_NetFwMgr);
    if (FAILED(hr))
    {
        DLOG(ERROR) << "CreateInstance() failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedComPtr<INetFwPolicy> firewall_policy;
    hr = firewall_manager->get_LocalPolicy(firewall_policy.Receive());
    if (FAILED(hr))
    {
        DLOG(ERROR) << "get_LocalPolicy() failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    hr = firewall_policy->get_CurrentProfile(current_profile_.Receive());
    if (FAILED(hr))
    {
        DLOG(ERROR) << "get_CurrentProfile() failed: " << SystemErrorCodeToString(hr);
        current_profile_ = nullptr;
        return false;
    }

    app_name_ = app_name;
    app_path_ = app_path;
    return true;
}

bool FirewallManagerLegacy::IsFirewallEnabled()
{
    VARIANT_BOOL is_enabled = VARIANT_TRUE;
    HRESULT hr = current_profile_->get_FirewallEnabled(&is_enabled);
    return SUCCEEDED(hr) && is_enabled != VARIANT_FALSE;
}

ScopedComPtr<INetFwAuthorizedApplications> FirewallManagerLegacy::GetAuthorizedApplications()
{
    ScopedComPtr<INetFwAuthorizedApplications> authorized_apps;

    HRESULT hr = current_profile_->get_AuthorizedApplications(authorized_apps.Receive());
    if (FAILED(hr))
    {
        DLOG(ERROR) << "get_AuthorizedApplications() failed: " << SystemErrorCodeToString(hr);
        return ScopedComPtr<INetFwAuthorizedApplications>();
    }

    return authorized_apps;
}

ScopedComPtr<INetFwAuthorizedApplication> FirewallManagerLegacy::CreateAuthorization(bool allow)
{
    ScopedComPtr<INetFwAuthorizedApplication> application;

    HRESULT hr = application.CreateInstance(CLSID_NetFwAuthorizedApplication);
    if (FAILED(hr))
    {
        DLOG(ERROR) << "CreateInstance() failed: " << SystemErrorCodeToString(hr);
        return ScopedComPtr<INetFwAuthorizedApplication>();
    }

    application->put_Name(ScopedBstr(app_name_.c_str()));
    application->put_ProcessImageFileName(ScopedBstr(app_path_.c_str()));
    // IpVersion defaults to NET_FW_IP_VERSION_ANY.
    // Scope defaults to NET_FW_SCOPE_ALL.
    // RemoteAddresses defaults to "*".
    application->put_Enabled(allow ? VARIANT_TRUE : VARIANT_FALSE);

    return application;
}

bool FirewallManagerLegacy::GetAllowIncomingConnection(bool& value)
{
    // Otherwise, check to see if there is a rule either allowing or disallowing
    // this chrome.exe.
    ScopedComPtr<INetFwAuthorizedApplications> authorized_apps(GetAuthorizedApplications());
    if (!authorized_apps.get())
        return false;

    ScopedComPtr<INetFwAuthorizedApplication> application;
    HRESULT hr = authorized_apps->Item(ScopedBstr(app_path_.c_str()),
                                       application.Receive());
    if (FAILED(hr))
        return false;

    VARIANT_BOOL is_enabled = VARIANT_FALSE;
    hr = application->get_Enabled(&is_enabled);
    if (FAILED(hr))
        return false;

    value = (is_enabled == VARIANT_TRUE);

    return true;
}

// The SharedAccess service must be running.
bool FirewallManagerLegacy::SetAllowIncomingConnection(bool allow)
{
    ScopedComPtr<INetFwAuthorizedApplications> authorized_apps(GetAuthorizedApplications());
    if (!authorized_apps.get())
        return false;

    // Authorize.
    ScopedComPtr<INetFwAuthorizedApplication> authorization = CreateAuthorization(allow);
    if (!authorization.get())
        return false;

    HRESULT hr = authorized_apps->Add(authorization.get());
    DLOG_IF(ERROR, FAILED(hr)) << "Add() failed: " << SystemErrorCodeToString(hr);

    return SUCCEEDED(hr);
}

void FirewallManagerLegacy::DeleteRule()
{
    ScopedComPtr<INetFwAuthorizedApplications> authorized_apps(GetAuthorizedApplications());
    if (!authorized_apps.get())
        return;

    authorized_apps->Remove(ScopedBstr(app_path_.c_str()));
}

} // namespace aspia
