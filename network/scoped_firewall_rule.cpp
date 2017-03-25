//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/scoped_firewall_rule.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/scoped_firewall_rule.h"
#include "network/firewall_manager.h"
#include "base/path.h"

namespace aspia {

static const WCHAR kAppName[] = L"Aspia Remote Desktop";
static const WCHAR kRuleName[] = L"Aspia Remote Desktop Host";
static const WCHAR kRuleDesc[] = L"Allow incoming connections";

ScopedFirewallRule::ScopedFirewallRule(uint16_t port)
{
    std::wstring exe_path;

    // Получаем полный путь к исполняемому файлу.
    if (!GetPath(PathKey::FILE_EXE, &exe_path))
        return;

    FirewallManager firewall_manager;

    if (firewall_manager.Init(kAppName, exe_path))
        firewall_manager.AddTcpRule(kRuleName, kRuleDesc, port);
}

ScopedFirewallRule::~ScopedFirewallRule()
{
    std::wstring exe_path;

    // Получаем полный путь к исполняемому файлу.
    if (!GetPath(PathKey::FILE_EXE, &exe_path))
        return;

    FirewallManager firewall_manager;

    if (firewall_manager.Init(kAppName, exe_path))
        firewall_manager.DeleteRule(kRuleName);
}

} // namespace aspia
