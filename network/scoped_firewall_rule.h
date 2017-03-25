//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/scoped_firewall_rule.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SCOPED_FIREWALL_RULE_H
#define _ASPIA_NETWORK__SCOPED_FIREWALL_RULE_H

#include "aspia_config.h"
#include "base/macros.h"

#include <stdint.h>

namespace aspia {

class ScopedFirewallRule
{
public:
    ScopedFirewallRule(uint16_t port);
    ~ScopedFirewallRule();

private:
    DISALLOW_COPY_AND_ASSIGN(ScopedFirewallRule);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SCOPED_FIREWALL_RULE_H
