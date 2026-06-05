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

#include "base/net/upnp_port_mapper.h"

#include <QHostAddress>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

#include <memory>
#include <string>

#include "base/logging.h"

namespace {

const char kMappingDescription[] = "Aspia Remote Desktop";
const char kLeaseDuration[] = "3600"; // Seconds. Renewed on each connection attempt.
const int kDiscoverDelayMs = 2000;

struct UpnpDevListDeleter
{
    void operator()(UPNPDev* device_list) const { freeUPNPDevlist(device_list); }
};

using ScopedUpnpDevList = std::unique_ptr<UPNPDev, UpnpDevListDeleter>;

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
// Returns true if |address| belongs to a private/non-routable range (RFC1918, CGNAT 100.64/10,
// link-local). In that case the IGD itself is behind another NAT and the mapping is useless.
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

} // namespace

//--------------------------------------------------------------------------------------------------
UpnpPortMapper::UpnpPortMapper(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UpnpPortMapper::~UpnpPortMapper()
{
    // The worker touches |this| (via invokeMethod) right before it returns, so joining here is what
    // keeps the object alive until that access is done. Any already-posted result event is dropped
    // by ~QObject afterwards.
    if (worker_.joinable())
        worker_.join();

    if (mapped_)
    {
        // Remove the mapping without blocking the destruction. The thread owns copies of the data
        // it needs, so it stays valid after this object is gone.
        std::thread(&UpnpPortMapper::doRemoveMapping, control_url_, service_type_, external_port_).detach();
    }
}

//--------------------------------------------------------------------------------------------------
void UpnpPortMapper::addUdpMapping(quint16 internal_port)
{
    if (worker_.joinable())
    {
        LOG(ERROR) << "Port mapping is already in progress";
        return;
    }

    worker_ = std::thread([this, internal_port]()
    {
        Result result = doMapping(internal_port);

        // Deliver the result on the thread this object lives in. If the object is destroyed before
        // delivery, Qt drops the queued call.
        QMetaObject::invokeMethod(this, [this, result]()
        {
            onMappingFinished(result);
        }, Qt::QueuedConnection);
    });
}

//--------------------------------------------------------------------------------------------------
// static
UpnpPortMapper::Result UpnpPortMapper::doMapping(quint16 internal_port)
{
    Result result;

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

    int code = UPNP_AddPortMapping(urls.value.controlURL, data.first.servicetype, port_str.c_str(),
        port_str.c_str(), lan_address, kMappingDescription, "UDP", nullptr, kLeaseDuration);
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
void UpnpPortMapper::doRemoveMapping(
    const std::string& control_url, const std::string& service_type, quint16 external_port)
{
    const std::string port_str = std::to_string(external_port);
    UPNP_DeletePortMapping(control_url.c_str(), service_type.c_str(), port_str.c_str(), "UDP", nullptr);
}

//--------------------------------------------------------------------------------------------------
void UpnpPortMapper::onMappingFinished(const Result& result)
{
    if (worker_.joinable())
        worker_.join();

    if (!result.success)
    {
        emit sig_failed();
        return;
    }

    mapped_ = true;
    external_port_ = result.external_port;
    control_url_ = result.control_url;
    service_type_ = result.service_type;

    LOG(INFO) << "UPnP mapping added:" << result.external_address << ":" << result.external_port;
    emit sig_ready(result.external_address, result.external_port);
}
