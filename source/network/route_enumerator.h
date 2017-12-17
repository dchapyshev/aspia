//
// PROJECT:         Aspia
// FILE:            network/route_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__ROUTE_ENUMERATOR_H
#define _ASPIA_NETWORK__ROUTE_ENUMERATOR_H

#include "base/macros.h"

#include <iphlpapi.h>
#include <memory>
#include <string>

namespace aspia {

class RouteEnumerator
{
public:
    RouteEnumerator();
    ~RouteEnumerator() = default;

    bool IsAtEnd() const;
    void Advance();

    std::string GetDestonation() const;
    std::string GetMask() const;
    std::string GetGateway() const;
    uint32_t GetMetric() const;

private:
    std::unique_ptr<uint8_t[]> forward_table_buffer_;
    PMIB_IPFORWARDTABLE forward_table_ = nullptr;
    DWORD index_ = 0;

    DISALLOW_COPY_AND_ASSIGN(RouteEnumerator);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__ROUTE_ENUMERATOR_H
