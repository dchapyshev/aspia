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

#ifndef BASE_NET_GATEWAY_PORT_MAPPER_H
#define BASE_NET_GATEWAY_PORT_MAPPER_H

#include <QObject>

#include "base/scoped_qpointer.h"

class NatPmpPortMapper;
class UpnpPortMapper;

// Orchestrates the gateway port-mapping mechanisms. NAT-PMP is tried first; on failure it falls back
// to UPnP. The winning mechanism's mapper is kept as a child and removes its mapping on destruction.
// On success sig_ready is emitted with the discovered external address and port; if both mechanisms
// fail, sig_failed is emitted.
class GatewayPortMapper final : public QObject
{
    Q_OBJECT

public:
    explicit GatewayPortMapper(QObject* parent = nullptr);
    ~GatewayPortMapper() final;

    void addUdpMapping(quint16 internal_port);

signals:
    void sig_ready(const QString& external_address, quint16 external_port);
    void sig_failed();

private:
    ScopedQPointer<NatPmpPortMapper> nat_pmp_mapper_;
    ScopedQPointer<UpnpPortMapper> upnp_mapper_;
    quint16 internal_port_ = 0;

    Q_DISABLE_COPY_MOVE(GatewayPortMapper)
};

#endif // BASE_NET_GATEWAY_PORT_MAPPER_H
