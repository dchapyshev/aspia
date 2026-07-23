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

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

#include <string>
#include <thread>

#include "base/logging.h"
#include "base/time_types.h"
#include "base/net/net_utils.h"

namespace {

const char kMappingDescription[] = "Aspia Remote Desktop";
const int kLeaseDurationSeconds = 3600;
const Milliseconds kDiscoverDelay{ 2000 };

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
    if (worker_context_)
    {
        // The worker posts its result only while |owner| is non-null under this mutex, so after
        // this point it never touches the destroyed mapper. The thread itself is detached and
        // finishes on its own.
        std::scoped_lock lock(worker_context_->mutex);
        worker_context_->owner = nullptr;
    }

    if (mapped_)
    {
        // Remove the mapping without blocking the destruction. The thread owns copies of the data it
        // needs, so it stays valid after this object is gone.
        std::thread(&UpnpPortMapper::doRemoveMapping, control_url_, service_type_, external_port_).detach();
    }
}

//--------------------------------------------------------------------------------------------------
void UpnpPortMapper::addUdpMapping(quint16 internal_port)
{
    if (worker_context_)
    {
        LOG(ERROR) << "UPnP mapping is already in progress";
        return;
    }

    worker_context_ = std::make_shared<WorkerContext>();
    worker_context_->owner = this;

    // The thread is detached: the destructor must not block on a discovery that takes seconds.
    // Posting under the context mutex guarantees the mapper is alive during the call (the
    // destructor blocks on the same mutex before nulling |owner|); the mapper as the context
    // object delivers the call on its own thread and Qt drops the posted call if the mapper dies
    // before delivery.
    std::thread([context = worker_context_, internal_port]()
    {
        Result result = doMapping(internal_port);

        std::scoped_lock lock(context->mutex);

        UpnpPortMapper* owner = context->owner;
        if (!owner)
            return;

        QMetaObject::invokeMethod(owner, [owner, result]()
        {
            owner->onMappingFinished(result);
        }, Qt::QueuedConnection);
    }).detach();
}

//--------------------------------------------------------------------------------------------------
// static
UpnpPortMapper::Result UpnpPortMapper::doMapping(quint16 internal_port)
{
    Result result;

    int error = 0;
    ScopedUpnpDevList device_list(upnpDiscover(static_cast<int>(kDiscoverDelay.count()), nullptr, nullptr, 0, 0, 2, &error));
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
    if (NetUtils::isPrivateIpAddress(external))
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
void UpnpPortMapper::doRemoveMapping(
    const std::string& control_url, const std::string& service_type, quint16 external_port)
{
    const std::string port_str = std::to_string(external_port);
    UPNP_DeletePortMapping(control_url.c_str(), service_type.c_str(), port_str.c_str(), "UDP", nullptr);
}

//--------------------------------------------------------------------------------------------------
void UpnpPortMapper::onMappingFinished(const Result& result)
{
    // The worker has finished; allow a new mapping request (its context copy keeps the shared
    // state alive until the thread exits).
    worker_context_.reset();

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
