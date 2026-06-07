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

#include "base/logging.h"
#include "base/net/pcp_port_mapper.h"
#include "base/net/upnp_port_mapper.h"

//--------------------------------------------------------------------------------------------------
GatewayPortMapper::GatewayPortMapper(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
GatewayPortMapper::~GatewayPortMapper() = default;

//--------------------------------------------------------------------------------------------------
void GatewayPortMapper::addUdpMapping(quint16 internal_port)
{
    if (pcp_mapper_ || upnp_mapper_)
    {
        LOG(ERROR) << "Port mapping is already in progress";
        return;
    }

    internal_port_ = internal_port;

    // Try PCP (with NAT-PMP fallback) first: it is fast and runs on the ASIO loop. UPnP is the
    // fallback on failure. The mapper that succeeds keeps the mapping alive and removes it on its
    // own destruction, so its sig_ready is forwarded straight to ours.
    pcp_mapper_ = new PcpPortMapper(this);

    connect(pcp_mapper_, &PcpPortMapper::sig_ready, this, &GatewayPortMapper::sig_ready);
    connect(pcp_mapper_, &PcpPortMapper::sig_failed, this, [this]()
    {
        // PCP/NAT-PMP failed; drop it and fall back to UPnP. Both UPnP outcomes are final, so
        // forward them straight through.
        pcp_mapper_.reset();

        upnp_mapper_ = new UpnpPortMapper(this);
        connect(upnp_mapper_, &UpnpPortMapper::sig_ready, this, &GatewayPortMapper::sig_ready);
        connect(upnp_mapper_, &UpnpPortMapper::sig_failed, this, &GatewayPortMapper::sig_failed);
        upnp_mapper_->addUdpMapping(internal_port_);
    });

    pcp_mapper_->addUdpMapping(internal_port);
}
