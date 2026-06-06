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

#include <memory>
#include <string>
#include <thread>

class GatewayPortMapper final : public QObject
{
    Q_OBJECT

public:
    explicit GatewayPortMapper(QObject* parent = nullptr);
    ~GatewayPortMapper() final;

    // Asynchronously asks the home gateway to forward its external |internal_port| to this host's
    // |internal_port|. NAT-PMP is tried first (fast, no discovery), then UPnP as a fallback. On
    // success sig_ready is emitted with the discovered external address and port; otherwise
    // sig_failed is emitted. The mapping is removed automatically when the object is destroyed.
    void addUdpMapping(quint16 internal_port);

signals:
    void sig_ready(const QString& external_address, quint16 external_port);
    void sig_failed();

private:
    enum class Type { NATPMP, UPNP };
    Q_ENUM(Type)

    struct Result
    {
        bool success = false;
        Type type = Type::NATPMP;
        QString external_address;
        quint16 external_port = 0;
        quint16 internal_port = 0; // NAT-PMP only: needed to drop the mapping.
        std::string control_url;   // UPnP only.
        std::string service_type;  // UPnP only.
    };

    static Result doNatPmpMapping(quint16 internal_port);
    static Result doUpnpMapping(quint16 internal_port);
    static void doRemoveNatPmpMapping(quint16 internal_port);
    static void doRemoveUpnpMapping(
        const std::string& control_url, const std::string& service_type, quint16 external_port);
    void onMappingFinished(const Result& result);

    std::thread worker_;
    std::shared_ptr<bool> alive_;

    bool mapped_ = false;
    Type type_ = Type::NATPMP;
    quint16 internal_port_ = 0;
    quint16 external_port_ = 0;
    std::string control_url_;
    std::string service_type_;

    Q_DISABLE_COPY_MOVE(GatewayPortMapper)
};

#endif // BASE_NET_GATEWAY_PORT_MAPPER_H
