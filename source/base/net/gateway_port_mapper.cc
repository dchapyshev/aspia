//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/gateway_port_mapper.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QtEndian>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#include <natpmp.h>

#if !defined(Q_OS_WINDOWS)
#include <sys/select.h>
#endif

#include <memory>
#include <string>

#include "base/logging.h"

namespace {

const char kMappingDescription[] = "Aspia Remote Desktop";
const int kLeaseDurationSeconds = 3600; // Renewed on each connection attempt.
const int kDiscoverDelayMs = 2000;

// NAT-PMP retransmits with an exponential backoff (250 ms, 500 ms, ...). A supporting gateway
// answers on the first probe, so a small cap keeps the fallback to UPnP fast when there is none.
const int kNatPmpMaxTries = 3;

//--------------------------------------------------------------------------------------------------
struct UpnpDevListDeleter
{
    void operator()(UPNPDev* device_list) const { freeUPNPDevlist(device_list); }
};

//--------------------------------------------------------------------------------------------------
using ScopedUpnpDevList = std::unique_ptr<UPNPDev, UpnpDevListDeleter>;

//--------------------------------------------------------------------------------------------------
// Owns the UPNPUrls populated by UPNP_GetValidIGD and frees them on scope exit. The struct itself
// lives on the stack; FreeUPNPUrls only releases the members the library allocated inside it.
struct ScopedUpnpUrls
{
    ScopedUpnpUrls() = default;
    ~ScopedUpnpUrls() { if (allocated) FreeUPNPUrls(&value); }

    ScopedUpnpUrls(const ScopedUpnpUrls&) = delete;
    ScopedUpnpUrls& operator=(const ScopedUpnpUrls&) = delete;

    UPNPUrls value {};
    bool allocated = false;
};

//--------------------------------------------------------------------------------------------------
struct ScopedNatPmp
{
    explicit ScopedNatPmp(natpmp_t* handle) : value(handle) {}
    ~ScopedNatPmp() { closenatpmp(value); }

    ScopedNatPmp(const ScopedNatPmp&) = delete;
    ScopedNatPmp& operator=(const ScopedNatPmp&) = delete;

    natpmp_t* value;
};

//--------------------------------------------------------------------------------------------------
// Returns true if |address| belongs to a private/non-routable range (RFC1918, CGNAT 100.64/10,
// link-local). In that case the gateway itself is behind another NAT and the mapping is useless.
bool isPrivateAddress(const QString& address)
{
    QHostAddress host_address(address);
    if (host_address.protocol() != QAbstractSocket::IPv4Protocol)
        return true;

    const quint32 ip = host_address.toIPv4Address();

    if ((ip & 0xFF000000) == 0x0A000000) // 10.0.0.0/8
        return true;
    if ((ip & 0xFFF00000) == 0xAC100000) // 172.16.0.0/12
        return true;
    if ((ip & 0xFFFF0000) == 0xC0A80000) // 192.168.0.0/16
        return true;
    if ((ip & 0xFFC00000) == 0x64400000) // 100.64.0.0/10 (CGNAT)
        return true;
    if ((ip & 0xFFFF0000) == 0xA9FE0000) // 169.254.0.0/16 (link-local)
        return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
// Waits for a single NAT-PMP response, driving the library's retransmission loop. Returns 0 on a
// received response, or a negative NATPMP_ERR_* / NATPMP_TRYAGAIN code on failure.
int readNatPmpResponse(natpmp_t* natpmp, natpmpresp_t* response)
{
    int result;
    int tries = 0;

    do
    {
        fd_set fds;
        struct timeval timeout;

        FD_ZERO(&fds);
        FD_SET(natpmp->s, &fds);
        getnatpmprequesttimeout(natpmp, &timeout);

        if (select(FD_SETSIZE, &fds, nullptr, nullptr, &timeout) < 0)
            return NATPMP_ERR_RECVFROM;

        result = readnatpmpresponseorretry(natpmp, response);
        if (result == NATPMP_TRYAGAIN && ++tries >= kNatPmpMaxTries)
            break;
    }
    while (result == NATPMP_TRYAGAIN);

    return result;
}

//--------------------------------------------------------------------------------------------------
// Queries the gateway for its external IPv4 address via NAT-PMP. Returns an empty string on failure.
QString natPmpExternalAddress(natpmp_t* natpmp)
{
    if (sendpublicaddressrequest(natpmp) < 0)
        return QString();

    natpmpresp_t response;
    if (readNatPmpResponse(natpmp, &response) != 0 || response.type != NATPMP_RESPTYPE_PUBLICADDRESS)
        return QString();

    // s_addr is in network byte order; QHostAddress(quint32) expects host order.
    const quint32 ip = qFromBigEndian<quint32>(response.pnu.publicaddress.addr.s_addr);
    return QHostAddress(ip).toString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
GatewayPortMapper::GatewayPortMapper(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
GatewayPortMapper::~GatewayPortMapper()
{
    // Do not block the owning thread: the worker never touches |this| directly (it delivers via the
    // application object guarded by |alive_|), so detaching is safe. |alive_| is released right after,
    // so a late worker result is dropped.
    if (worker_.joinable())
        worker_.detach();

    if (mapped_)
    {
        // Remove the mapping without blocking the destruction. The thread owns copies of the data
        // it needs, so it stays valid after this object is gone.
        if (type_ == Type::NATPMP)
        {
            std::thread(&GatewayPortMapper::doRemoveNatPmpMapping, internal_port_).detach();
        }
        else
        {
            std::thread(&GatewayPortMapper::doRemoveUpnpMapping,
                        control_url_, service_type_, external_port_).detach();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void GatewayPortMapper::addUdpMapping(quint16 internal_port)
{
    if (worker_.joinable())
    {
        LOG(ERROR) << "Port mapping is already in progress";
        return;
    }

    alive_ = std::make_shared<bool>(true);
    std::weak_ptr<bool> guard = alive_;

    worker_ = std::thread([this, guard, internal_port]()
    {
        // NAT-PMP is fast and needs no discovery, so try it first; fall back to UPnP otherwise.
        Result result = doNatPmpMapping(internal_port);
        if (!result.success)
            result = doUpnpMapping(internal_port);

        // Deliver on the owning thread via the always-alive application object, so the worker never
        // touches |this| directly. The guard, checked on that thread, drops the result if the mapper
        // was destroyed meanwhile.
        QMetaObject::invokeMethod(qApp, [this, guard, result]()
        {
            if (guard.expired())
                return;
            onMappingFinished(result);
        }, Qt::QueuedConnection);
    });
}

//--------------------------------------------------------------------------------------------------
// static
GatewayPortMapper::Result GatewayPortMapper::doNatPmpMapping(quint16 internal_port)
{
    Result result;
    result.type = Type::NATPMP;

    natpmp_t natpmp;
    if (initnatpmp(&natpmp, 0, 0) != 0) // 0, 0: let the library autodetect the default gateway.
    {
        LOG(INFO) << "No NAT-PMP gateway detected";
        return result;
    }

    ScopedNatPmp scoped_natpmp(&natpmp);

    QString external = natPmpExternalAddress(&natpmp);
    if (external.isEmpty())
    {
        LOG(INFO) << "Gateway does not answer NAT-PMP";
        return result;
    }

    if (isPrivateAddress(external))
    {
        LOG(INFO) << "NAT-PMP external address is private (double NAT/CGNAT), useless";
        return result;
    }

    if (sendnewportmappingrequest(
            &natpmp, NATPMP_PROTOCOL_UDP, internal_port, internal_port, kLeaseDurationSeconds) < 0)
    {
        LOG(WARNING) << "sendnewportmappingrequest failed";
        return result;
    }

    natpmpresp_t response;
    if (readNatPmpResponse(&natpmp, &response) != 0 || response.type != NATPMP_RESPTYPE_UDPPORTMAPPING)
    {
        LOG(WARNING) << "NAT-PMP port mapping request was not granted";
        return result;
    }

    result.success = true;
    result.external_address = external;
    result.external_port = response.pnu.newportmapping.mappedpublicport;
    result.internal_port = internal_port;

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
GatewayPortMapper::Result GatewayPortMapper::doUpnpMapping(quint16 internal_port)
{
    Result result;
    result.type = Type::UPNP;

    int error = 0;
    ScopedUpnpDevList device_list(upnpDiscover(kDiscoverDelayMs, nullptr, nullptr, 0, 0, 2, &error));
    if (!device_list)
    {
        LOG(INFO) << "No UPnP devices discovered (error:" << error << ")";
        return result;
    }

    ScopedUpnpUrls urls;
    IGDdatas data;
    char lan_address[64] = { 0 };
    char wan_address[64] = { 0 };

    int igd = UPNP_GetValidIGD(device_list.get(), &urls.value, &data,
        lan_address, sizeof(lan_address), wan_address, sizeof(wan_address));

    // A non-zero result means UPNP_GetValidIGD allocated the urls; mark them for release on exit.
    urls.allocated = (igd != 0);

    // 1 means a connected IGD was found.
    if (igd != 1)
    {
        LOG(INFO) << "No connected UPnP IGD found (code:" << igd << ")";
        return result;
    }

    char external_address[40] = { 0 };
    if (UPNP_GetExternalIPAddress(urls.value.controlURL, data.first.servicetype, external_address) !=
        UPNPCOMMAND_SUCCESS)
    {
        LOG(WARNING) << "Unable to get external IP address from IGD";
        return result;
    }

    QString external = QString::fromLatin1(external_address);
    if (isPrivateAddress(external))
    {
        LOG(INFO) << "IGD external address is private (double NAT/CGNAT), UPnP is useless";
        return result;
    }

    const std::string port_str = std::to_string(internal_port);
    const std::string lease_str = std::to_string(kLeaseDurationSeconds);

    int code = UPNP_AddPortMapping(urls.value.controlURL, data.first.servicetype, port_str.c_str(),
        port_str.c_str(), lan_address, kMappingDescription, "UDP", nullptr, lease_str.c_str());
    if (code != UPNPCOMMAND_SUCCESS)
    {
        LOG(WARNING) << "UPNP_AddPortMapping failed:" << strupnperror(code);
        return result;
    }

    result.success = true;
    result.external_address = external;
    result.external_port = internal_port;
    result.control_url = urls.value.controlURL;
    result.service_type = data.first.servicetype;

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
void GatewayPortMapper::doRemoveNatPmpMapping(quint16 internal_port)
{
    natpmp_t natpmp;
    if (initnatpmp(&natpmp, 0, 0) != 0)
        return;

    ScopedNatPmp scoped_natpmp(&natpmp);

    // A lease time of 0 removes the mapping; the public port is ignored for deletion.
    if (sendnewportmappingrequest(&natpmp, NATPMP_PROTOCOL_UDP, internal_port, 0, 0) < 0)
        return;

    natpmpresp_t response;
    readNatPmpResponse(&natpmp, &response);
}

//--------------------------------------------------------------------------------------------------
// static
void GatewayPortMapper::doRemoveUpnpMapping(
    const std::string& control_url, const std::string& service_type, quint16 external_port)
{
    const std::string port_str = std::to_string(external_port);
    UPNP_DeletePortMapping(control_url.c_str(), service_type.c_str(), port_str.c_str(), "UDP", nullptr);
}

//--------------------------------------------------------------------------------------------------
void GatewayPortMapper::onMappingFinished(const Result& result)
{
    if (worker_.joinable())
        worker_.join();

    if (!result.success)
    {
        emit sig_failed();
        return;
    }

    mapped_ = true;
    type_ = result.type;
    internal_port_ = result.internal_port;
    external_port_ = result.external_port;
    control_url_ = result.control_url;
    service_type_ = result.service_type;

    LOG(INFO) << result.type << "mapping added:" << result.external_address << ":" << result.external_port;
    emit sig_ready(result.external_address, result.external_port);
}
