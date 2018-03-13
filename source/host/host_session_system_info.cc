//
// PROJECT:         Aspia
// FILE:            host/host_session_system_info.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_system_info.h"

#include <QDebug>
#include <QtEndian>

#include "base/process/process.h"
#include "codec/compressor_zlib.h"
#include "ipc/pipe_channel_proxy.h"
#include "proto/auth_session.pb.h"
#include "proto/system_info_session.pb.h"
#include "protocol/message_serialization.h"

namespace aspia {

namespace {

static constexpr size_t kGuidLength = 38;

// If the size of the received data exceeds this value, then the data will not be transferred.
static constexpr size_t kMaxMessageSize = 8 * 1024 * 1024; // 8 MB

// If the message size exceeds |kMaxUncompressedMessageSize|, then the data is compressed.
static constexpr size_t kMaxUncompressedMessageSize = 4 * 1024; // 4 kB

// The compression ratio can be in the range of 1 to 9.
static constexpr int32_t kCompressionRatio = 6;

void CompressMessageData(const std::string& input_data, std::string* output_data)
{
    size_t output_data_size = input_data.size() + input_data.size() / 100 + 16;

    output_data->resize(output_data_size + sizeof(uint32_t));

    uint8_t* output_buffer = reinterpret_cast<uint8_t*>(output_data->data());
    const uint8_t* input_buffer = reinterpret_cast<const uint8_t*>(input_data.data());

    // The first 4 bytes store the size of the original (uncompressed) buffer.
    *reinterpret_cast<uint32_t*>(output_buffer) =
        qToBigEndian(static_cast<uint32_t>(input_data.size()));
    output_buffer += sizeof(uint32_t);

    size_t input_buffer_pos = 0;
    size_t output_buffer_pos = 0;

    bool compress_again = true;

    CompressorZLIB compressor(kCompressionRatio);

    while (compress_again)
    {
        Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;

        if (input_buffer_pos == input_data.size())
            flush = Compressor::CompressorFinish;

        size_t consumed = 0;
        size_t written = 0;

        compress_again = compressor.Process(
            input_buffer + input_buffer_pos, input_data.size() - input_buffer_pos,
            output_buffer + output_buffer_pos, output_data_size - output_buffer_pos,
            flush, &consumed, &written);

        input_buffer_pos += consumed;
        output_buffer_pos += written;

        // If we have filled the message or we have reached the end of stream.
        if (output_buffer_pos == output_data_size || !compress_again)
        {
            output_data->resize(output_buffer_pos);
            return;
        }
    }
}

} // namespace

void HostSessionSystemInfo::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (!ipc_channel_)
        return;

    ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

    uint32_t user_data = Process::Current().Pid();

    if (ipc_channel_->Connect(user_data))
    {
        OnIpcChannelConnect(user_data);

        // Waiting for the connection to close.
        ipc_channel_proxy_->WaitForDisconnect();
    }
}

void HostSessionSystemInfo::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::auth::SessionType session_type = static_cast<proto::auth::SessionType>(user_data);

    Q_ASSERT(session_type == proto::auth::SESSION_TYPE_SYSTEM_INFO);

    map_ = std::move(CreateCategoryMap());

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionSystemInfo::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionSystemInfo::OnIpcChannelMessage(const QByteArray& buffer)
{
    proto::system_info::ClientToHost request;

    if (ParseMessage(buffer, request))
    {
        const std::string& guid = request.guid();

        if (guid.length() == kGuidLength && guid.front() == '{' && guid.back() == '}')
        {
            proto::system_info::HostToClient reply;

            reply.set_guid(guid);

            // Looking for a category by GUID.
            const auto category = map_.find(guid);
            if (category != map_.end())
            {
                std::string data = category->second->Serialize();

                if (data.size() > kMaxUncompressedMessageSize)
                {
                    reply.set_compressor(proto::system_info::HostToClient::COMPRESSOR_ZLIB);
                    CompressMessageData(data, reply.mutable_data());
                }
                else
                {
                    reply.set_compressor(proto::system_info::HostToClient::COMPRESSOR_RAW);
                    reply.set_data(std::move(data));
                }
            }

            size_t message_size = reply.ByteSizeLong();

            if (message_size > kMaxMessageSize)
            {
                qWarning() << "Too much data: " << message_size;
                reply.clear_data();
            }

            // Sending response to the request.
            ipc_channel_proxy_->Send(
                SerializeMessage(reply),
                std::bind(&HostSessionSystemInfo::OnIpcChannelMessageSended, this));
            return;
        }
    }

    ipc_channel_proxy_->Disconnect();
}

void HostSessionSystemInfo::OnIpcChannelMessageSended()
{
    // The response to the request was sent. Now we can receive the following request.
    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionSystemInfo::OnIpcChannelMessage, this, std::placeholders::_1));
}

} // namespace aspia
