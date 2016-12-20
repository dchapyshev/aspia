/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/client.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CLIENT__CLIENT_H
#define _ASPIA_CLIENT__CLIENT_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/thread.h"
#include "base/lock.h"
#include "base/scoped_aligned_buffer.h"
#include "network/socket_tcp.h"
#include "codec/video_decoder_zlib.h"
#include "codec/video_decoder_vp8.h"
#include "desktop_capture/desktop_region.h"
#include "client/message_queue.h"
#include "crypto/encryptor_aes.h"
#include "crypto/decryptor_aes.h"

namespace aspia {

class Client : private Thread
{
public:
    enum class Event
    {
        Connected,    // Успешно подключен к серверу.
        Disconnected, // Отключен от сервера.
        NotConnected, // Не удалось подключиться.
        BadAuth,       // Не удалось пройти авторизацию.
        VideoUpdate
    };

    typedef std::function<void(Client::Event)> OnEventCallback;

    Client(std::unique_ptr<Socket> sock, OnEventCallback on_event_callback);
    ~Client();

    void SendPointerEvent(int32_t x, int32_t y, int32_t mask);
    void SendKeyEvent(int32_t keycode, bool extended, bool pressed);

    void SendVideoControl(bool enable,
                          int32_t encoding,
                          const PixelFormat &pixel_format);

    typedef std::function<void(const uint8_t*, const DesktopRegion&)> OnVideoUpdateCallback;
    typedef std::function<void(const DesktopSize&, const PixelFormat&)> OnVideoResizeCallback;

    void EnableVideoUpdate(int32_t encoding,
                           const PixelFormat &pixel_format,
                           OnVideoUpdateCallback on_update_callback,
                           OnVideoResizeCallback on_resize_callback);

private:
    void Worker() override;
    void OnStop() override;

    void AsyncNotify(Client::Event type);

    void DoKeyExchange();
    void WriteMessage(const proto::ClientToHost *message);
    void ReadMessage(std::unique_ptr<proto::HostToClient> *message);
    void ProcessMessage(const proto::HostToClient *message);

    void ReadVideoPacket(const proto::VideoPacket &packet);

private:
    OnEventCallback on_event_callback_;

    OnVideoUpdateCallback on_video_update_callback_;
    OnVideoResizeCallback on_video_resize_callback_;

    std::unique_ptr<Socket> sock_;
    std::unique_ptr<MessageQueue<proto::HostToClient>> input_queue_;
    std::unique_ptr<MessageQueue<proto::ClientToHost>> output_queue_;
    std::unique_ptr<VideoDecoder> decoder_;

    int32_t encoding_;

    DesktopSize screen_size_;
    PixelFormat pixel_format_;
    DesktopRegion changed_region_;

    std::unique_ptr<Encryptor> encryptor_;
    std::unique_ptr<Decryptor> decryptor_;

    uint32_t write_buffer_size_;
    uint32_t read_buffer_size_;

    std::unique_ptr<ScopedAlignedBuffer> write_buffer_;
    std::unique_ptr<ScopedAlignedBuffer> read_buffer_;

    Lock write_lock_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
