//
// PROJECT:         Aspia
// FILE:            client/client_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/byte_order.h"
#include "client/client_session_system_info.h"
#include "codec/decompressor_zlib.h"
#include "report/report_creator_proxy.h"
#include "proto/system_info_session.pb.h"
#include "protocol/message_serialization.h"

namespace aspia {

namespace {

constexpr size_t kGuidLength = 38;

void UncompressMessageData(const std::string& input_data, std::string* output_data)
{
    if (input_data.size() < sizeof(uint32_t))
        return;

    size_t input_data_size = input_data.size() - sizeof(uint32_t);

    if (!input_data_size)
        return;

    // The first 4 bytes store the size of the original (uncompressed) buffer.
    size_t output_data_size = NetworkByteOrderToHost(
        *reinterpret_cast<const uint32_t*>(input_data.data()));

    if (!output_data_size)
        return;

    output_data->resize(output_data_size);

    const uint8_t* input_buffer = reinterpret_cast<const uint8_t*>(input_data.data()) + sizeof(uint32_t);
    uint8_t* output_buffer = reinterpret_cast<uint8_t*>(output_data->data());

    size_t input_buffer_pos = 0;
    size_t output_buffer_pos = 0;

    bool decompress_again = true;

    DecompressorZLIB decompressor;

    while (decompress_again && input_buffer_pos < input_data_size)
    {
        size_t written = 0;
        size_t consumed = 0;

        decompress_again = decompressor.Process(input_buffer + input_buffer_pos,
                                                input_data_size - input_buffer_pos,
                                                output_buffer + output_buffer_pos,
                                                output_data_size - output_buffer_pos,
                                                &consumed,
                                                &written);
        input_buffer_pos += consumed;
        output_buffer_pos += written;
    }
}

} // namespace

ClientSessionSystemInfo::ClientSessionSystemInfo(
    const ClientConfig& config,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : ClientSession(config, channel_proxy)
{
    window_.reset(new SystemInfoWindow(this));
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    window_.reset();
}

void ClientSessionSystemInfo::OnRequest(
    std::string_view guid, std::shared_ptr<ReportCreatorProxy> report_creator)
{
    report_creator_ = std::move(report_creator);

    proto::system_info::ClientToHost message;
    message.set_guid(guid.data());

    channel_proxy_->Send(SerializeMessage<IOBuffer>(message),
                         std::bind(&ClientSessionSystemInfo::OnRequestSended, this));
}

void ClientSessionSystemInfo::OnWindowClose()
{
    channel_proxy_->Disconnect();
}

void ClientSessionSystemInfo::OnRequestSended()
{
    channel_proxy_->Receive(std::bind(
        &ClientSessionSystemInfo::OnReplyReceived, this, std::placeholders::_1));
}

void ClientSessionSystemInfo::OnReplyReceived(const IOBuffer& buffer)
{
    proto::system_info::HostToClient message;

    if (ParseMessage(buffer, message))
    {
        const std::string& guid = message.guid();

        if (guid.length() == kGuidLength && guid.front() == '{' && guid.back() == '}')
        {
            switch (message.compressor())
            {
                case proto::system_info::HostToClient::COMPRESSOR_ZLIB:
                {
                    std::string data;
                    UncompressMessageData(message.data(), &data);
                    report_creator_->Parse(data);
                    return;
                }

                case proto::system_info::HostToClient::COMPRESSOR_RAW:
                {
                    report_creator_->Parse(message.data());
                    return;
                }

                default:
                    LOG(ERROR) << "Unknown compressor: " << message.compressor();
                    break;
            }
        }
        else
        {
            DLOG(ERROR) << "Invalid GUID: " << guid;
        }
    }

    channel_proxy_->Disconnect();
}

} // namespace aspia
