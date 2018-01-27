//
// PROJECT:         Aspia
// FILE:            network/network_adapter_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_ADAPTER_ENUMERATOR_H
#define _ASPIA_NETWORK__NETWORK_ADAPTER_ENUMERATOR_H

#include "base/macros.h"

#include <iphlpapi.h>
#include <memory>
#include <string>

namespace aspia {

class NetworkAdapterEnumerator
{
public:
    NetworkAdapterEnumerator();
    ~NetworkAdapterEnumerator() = default;

    bool IsAtEnd() const;
    void Advance();

    std::string GetAdapterName() const;
    std::string GetConnectionName() const;
    std::string GetInterfaceType() const;
    uint32_t GetMtu() const;
    uint32_t GetSpeed() const;
    std::string GetMacAddress() const;
    bool IsWinsEnabled() const;
    std::string GetPrimaryWinsServer() const;
    std::string GetSecondaryWinsServer() const;
    bool IsDhcpEnabled() const;

    class IpAddressEnumerator
    {
    public:
        explicit IpAddressEnumerator(const NetworkAdapterEnumerator& adapter);

        bool IsAtEnd() const { return address_ == nullptr; }
        void Advance();

        std::string GetIpAddress() const;
        std::string GetIpMask() const;

    private:
        PIP_ADDR_STRING address_;

        DISALLOW_COPY_AND_ASSIGN(IpAddressEnumerator);
    };

    class GatewayEnumerator
    {
    public:
        explicit GatewayEnumerator(const NetworkAdapterEnumerator& adapter);

        bool IsAtEnd() const { return address_ == nullptr; }
        void Advance();
        std::string GetAddress() const;

    private:
        PIP_ADDR_STRING address_;

        DISALLOW_COPY_AND_ASSIGN(GatewayEnumerator);
    };

    class DhcpEnumerator
    {
    public:
        explicit DhcpEnumerator(const NetworkAdapterEnumerator& adapter);

        bool IsAtEnd() const { return address_ == nullptr; }
        void Advance();
        std::string GetAddress() const;

    private:
        PIP_ADDR_STRING address_;

        DISALLOW_COPY_AND_ASSIGN(DhcpEnumerator);
    };

    class DnsEnumerator
    {
    public:
        explicit DnsEnumerator(const NetworkAdapterEnumerator& adapter);

        bool IsAtEnd() const { return address_ == nullptr; }
        void Advance();
        std::string GetAddress() const;

    private:
        std::unique_ptr<uint8_t[]> info_buffer_;
        PIP_ADDR_STRING address_ = nullptr;

        DISALLOW_COPY_AND_ASSIGN(DnsEnumerator);
    };

private:
    std::unique_ptr<uint8_t[]> adapters_buffer_;
    PIP_ADAPTER_INFO adapter_;

    DISALLOW_COPY_AND_ASSIGN(NetworkAdapterEnumerator);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_ADAPTER_ENUMERATOR_H
