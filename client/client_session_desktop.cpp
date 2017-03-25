//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_desktop.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_desktop.h"
#include "base/logging.h"

namespace aspia {

ClientSessionDesktop::ClientSessionDesktop(std::unique_ptr<Socket> sock) :
    ClientSession(std::move(sock)),
    video_encoding_(proto::VIDEO_ENCODING_UNKNOWN)
{
    //
    // Обработка сообщения (в частности декодирование видео-пакетов) может занимать
    // длительное время. Для того, чтобы не блокировать поток чтения пакетов из сети
    // мы создаем отдельные потоки для обработки сообщений. Сообщения добавляются в
    // очередь и обрабатываются в порядке поступления из данных потоков.
    //
    MessageQueue<proto::desktop::HostToClient>::ProcessMessageCallback process_input_message_callback =
        std::bind(&ClientSessionDesktop::ProcessIncommingMessage, this, std::placeholders::_1);

    incomming_queue_.reset(new MessageQueue<proto::desktop::HostToClient>(process_input_message_callback));

    MessageQueue<proto::desktop::ClientToHost>::ProcessMessageCallback process_output_message_callback =
        std::bind(&ClientSessionDesktop::WriteMessage, this, std::placeholders::_1);

    outgoing_queue_.reset(new MessageQueue<proto::desktop::ClientToHost>(process_output_message_callback));
}

void ClientSessionDesktop::ConfigureSession(const ScreenConfig& screen_config)
{
    uint32_t flags = 0;

    flags |= screen_config.DesktopEffects() ? proto::DesktopConfig::ENABLE_DESKTOP_EFFECTS : 0;
    flags |= screen_config.CursorShape() ? proto::DesktopConfig::ENABLE_CURSOR_SHAPE : 0;
    flags |= screen_config.Clipboard() ? proto::DesktopConfig::ENABLE_CLIPBOARD : 0;

    std::unique_ptr<proto::desktop::ClientToHost> message(new proto::desktop::ClientToHost());

    proto::DesktopConfig* config = message->mutable_config();
    config->set_flags(flags);
    config->set_encoding(screen_config.Encoding());
    config->set_update_interval(screen_config.UpdateInterval());

    //
    // Поле формата пикселей применимо на данный момент только для
    // данных кодировок, а для остальных - игнорируется.
    //
    if (screen_config.Encoding() == proto::VIDEO_ENCODING_ZLIB)
    {
        config->set_compress_ratio(screen_config.CompressRatio());
        screen_config.Format().ToVideoPixelFormat(config->mutable_pixel_format());
    }

    if (screen_config.Clipboard())
    {
        Clipboard::ClipboardEventCallback clipboard_event_callback =
            std::bind(&ClientSessionDesktop::SendClipboardEvent, this, std::placeholders::_1);

        clipboard_.StartExchange(clipboard_event_callback);
    }
    else
    {
        clipboard_.StopExchange();
    }

    outgoing_queue_->Add(std::move(message));
}

bool ClientSessionDesktop::ReadVideoPacket(const proto::VideoPacket& packet)
{
    // Переинициализируем декодер при необходимости
    if (video_encoding_ != packet.encoding())
    {
        video_encoding_ = packet.encoding();

        switch (video_encoding_)
        {
            case proto::VIDEO_ENCODING_VP8:
                video_decoder_.reset(new VideoDecoderVP8());
                break;

            case proto::VIDEO_ENCODING_VP9:
                video_decoder_.reset(new VideoDecoderVP9());
                break;

            case proto::VIDEO_ENCODING_ZLIB:
                video_decoder_.reset(new VideoDecoderZLIB());
                break;

            default:
                LOG(ERROR) << "Unsupported encoding: " << video_encoding_;
                return false;
        }
    }

    if (packet.has_screen_size() || packet.has_pixel_format())
    {
        PixelFormat pixel_format(packet.pixel_format());

        if (pixel_format.IsEmpty())
            pixel_format = PixelFormat::ARGB();

        ResizeVideoFrame(DesktopSize(packet.screen_size()), pixel_format);
    }

    DesktopFrame* frame = GetVideoFrame();
    if (!frame)
        return false;

    if (!video_decoder_->Decode(packet, frame))
        return false;

    VideoFrameUpdated();

    return true;
}

bool ClientSessionDesktop::ReadCursorShape(const proto::CursorShape& cursor_shape)
{
    // Если декодер курсора не инициализирован.
    if (!cursor_decoder_)
    {
        // Инициализируем его.
        cursor_decoder_.reset(new CursorDecoder());
    }

    // Декодируем изображение курсора.
    const MouseCursor* mouse_cursor = cursor_decoder_->Decode(cursor_shape);

    if (mouse_cursor)
    {
        OnCursorUpdate(mouse_cursor);
        return true;
    }

    return false;
}

bool ClientSessionDesktop::ReadClipboardEvent(const proto::ClipboardEvent& event)
{
    clipboard_.InjectClipboardEvent(event);
    return true;
}

bool ClientSessionDesktop::ReadConfigRequest(const proto::DesktopConfigRequest& config_request)
{
    OnConfigRequest();
    return true;
}

void ClientSessionDesktop::SendPointerEvent(int32_t x, int32_t y, uint32_t mask)
{
    std::unique_ptr<proto::desktop::ClientToHost> message(new proto::desktop::ClientToHost());

    proto::PointerEvent* event = message->mutable_pointer_event();
    event->set_x(x);
    event->set_y(y);
    event->set_mask(mask);

    outgoing_queue_->Add(std::move(message));
}

void ClientSessionDesktop::SendKeyEvent(uint32_t keycode, uint32_t flags)
{
    std::unique_ptr<proto::desktop::ClientToHost> message(new proto::desktop::ClientToHost());

    proto::KeyEvent* event = message->mutable_key_event();
    event->set_keycode(keycode);
    event->set_flags(flags);

    outgoing_queue_->Add(std::move(message));
}

void ClientSessionDesktop::SendPowerEvent(proto::PowerEvent::Action action)
{
    std::unique_ptr<proto::desktop::ClientToHost> message(new proto::desktop::ClientToHost());
    message->mutable_power_event()->set_action(action);
    outgoing_queue_->Add(std::move(message));
}

void ClientSessionDesktop::SendClipboardEvent(const proto::ClipboardEvent& event)
{
    std::unique_ptr<proto::desktop::ClientToHost> message(new proto::desktop::ClientToHost());
    message->mutable_clipboard_event()->CopyFrom(event);
    outgoing_queue_->Add(std::move(message));
}

bool ClientSessionDesktop::ProcessIncommingMessage(const proto::desktop::HostToClient* message)
{
    //
    // Сообщение хоста может содержать одновременно все типы сообщений.
    // Неизвестные типы сообщений игнорируются.
    //

    if (message->has_video_packet())
    {
        if (!ReadVideoPacket(message->video_packet()))
            return false;
    }

    if (message->has_cursor_shape())
    {
        if (!ReadCursorShape(message->cursor_shape()))
            return false;
    }

    if (message->has_clipboard_event())
    {
        if (!ReadClipboardEvent(message->clipboard_event()))
            return false;
    }

    if (message->has_config_request())
    {
        if (!ReadConfigRequest(message->config_request()))
            return false;
    }

    return true;
}

bool ClientSessionDesktop::OnIncommingMessage(const uint8_t* buffer, uint32_t size)
{
    // В данный метод всегда должно передаваться корректное сообщение.
    DCHECK(buffer && size);

    std::unique_ptr<proto::desktop::HostToClient> message(new proto::desktop::HostToClient());

    if (!message->ParseFromArray(buffer, size))
    {
        LOG(ERROR) << "ParseFromArray() failed";
        return false;
    }

    // Добавляем сообщение в очередь.
    incomming_queue_->Add(std::move(message));

    return true;
}

bool ClientSessionDesktop::WriteMessage(const proto::desktop::ClientToHost* message)
{
    uint32_t size = message->ByteSizeLong();

    if (!size)
        return false;

    if (outgoing_buffer_.Size() < size)
        outgoing_buffer_.Resize(size);

    message->SerializeWithCachedSizesToArray(outgoing_buffer_);

    return WriteOutgoingMessage(outgoing_buffer_, size);
}

} // namespace aspia
