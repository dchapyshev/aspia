//
// PROJECT:         Aspia
// FILE:            network/open_connection_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__OPEN_CONNECTION_ENUMERATOR_H
#define _ASPIA_NETWORK__OPEN_CONNECTION_ENUMERATOR_H

#include "base/scoped_object.h"
#include "proto/system_info_session_message.pb.h"

#include <iphlpapi.h>
#include <memory>
#include <string>

namespace aspia {

class OpenConnectionEnumerator
{
public:
    enum class Type { UDP, TCP };

    OpenConnectionEnumerator(Type type);

    bool IsAtEnd() const;
    void Advance();

    std::string GetProcessName() const;
    std::string GetLocalAddress() const;
    std::string GetRemoteAddress() const;
    uint16_t GetLocalPort() const;
    uint16_t GetRemotePort() const;
    proto::OpenConnections::State GetState() const;

private:
    void InitializeTcpTable();
    void InitializeUdpTable();

    ScopedHandle process_snapshot_;

    std::unique_ptr<uint8_t[]> table_buffer_;
    PMIB_TCPTABLE_OWNER_PID tcp_table_ = nullptr;
    PMIB_UDPTABLE_OWNER_PID udp_table_ = nullptr;
    ULONG index_ = 0;

    DISALLOW_COPY_AND_ASSIGN(OpenConnectionEnumerator);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__OPEN_CONNECTION_ENUMERATOR_H
