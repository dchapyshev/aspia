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

#include "host/android/desktop_agent.h"

#include <QTimer>

#include "base/logging.h"
#include "base/codec/scale_reducer.h"
#include "base/codec/video_encoder.h"
#include "base/desktop/frame.h"
#include "base/desktop/region.h"
#include "common/clipboard.h"
#include "host/screen_capturer.h"
#include "host/android/audio_capturer_android.h"
#include "host/android/desktop_client.h"
#include "host/android/floating_menu_bridge.h"
#include "host/android/input_injector_android.h"
#include "host/android/screen_capturer_android.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_input.h"
#include "proto/desktop_screen.h"

namespace {

// Target capture cadence. A fixed interval is enough for the Android host; the desktop's adaptive
// scheduler is not ported here.
const int kCaptureIntervalMs = 33;

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopAgent::DesktopAgent(QObject* parent)
    : QObject(parent),
      capture_timer_(new QTimer(this)),
      scale_reducer_(std::make_unique<ScaleReducer>(ScaleReducer::Quality::NORMAL)),
      video_encoding_(proto::video::ENCODING_VP8),
      h264_enabled_(VideoEncoder::isSupported(proto::video::ENCODING_H264))
{
    LOG(INFO) << "Ctor";

    capture_timer_->setSingleShot(true);
    capture_timer_->setTimerType(Qt::PreciseTimer);
    connect(capture_timer_, &QTimer::timeout, this, &DesktopAgent::onCaptureScreen);

    // Clipboard flows through the floating overlay button (background clipboard access is blocked).
    floating_menu_bridge_ = new FloatingMenuBridge(this);
    connect(floating_menu_bridge_, &FloatingMenuBridge::sig_clipboardText,
            this, &DesktopAgent::onClipboardTextChanged);
}

//--------------------------------------------------------------------------------------------------
DesktopAgent::~DesktopAgent()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::addClient(DesktopClient* client)
{
    // The agent owns its desktop clients; it deletes them when they finish (onClientFinished).
    client->setParent(this);
    clients_.append(client);

    connect(client, &DesktopClient::sig_configured, this, &DesktopAgent::onClientConfigured);
    connect(client, &DesktopClient::sig_keyFrameRequested, this, &DesktopAgent::onKeyFrameRequested);
    connect(client, &DesktopClient::sig_preferredSizeChanged, this, &DesktopAgent::onPreferredSizeChanged);
    connect(client, &DesktopClient::sig_screenListRequested, this, &DesktopAgent::onScreenListRequested);
    connect(client, &DesktopClient::sig_injectKeyEvent, this, &DesktopAgent::onInjectKeyEvent);
    connect(client, &DesktopClient::sig_injectTextEvent, this, &DesktopAgent::onInjectTextEvent);
    connect(client, &DesktopClient::sig_injectMouseEvent, this, &DesktopAgent::onInjectMouseEvent);
    connect(client, &DesktopClient::sig_injectTouchEvent, this, &DesktopAgent::onInjectTouchEvent);
    connect(client, &Client::sig_finished, this, &DesktopAgent::onClientFinished);

    connect(client, &DesktopClient::sig_injectClipboardEvent, this, &DesktopAgent::onInjectClipboardEvent);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClientConfigured()
{
    bool has_configured = false;
    bool vp8_supported = true;
    bool vp9_supported = true;
    bool h264_supported = true;

    // A codec is usable only if every configured client can decode it.
    for (auto* client : std::as_const(clients_))
    {
        if (!client->isConfigured())
            continue;

        has_configured = true;

        if (!client->isVp8Supported())
            vp8_supported = false;
        if (!client->isVp9Supported())
            vp9_supported = false;
        if (!client->isH264Supported())
            h264_supported = false;
    }

    if (!has_configured)
        return;

    // Prefer hardware H264 when both endpoints support it; fall back to VP otherwise. The explicit
    // reset of an H264 encoding handles a client revoking the capability mid-session (decoder
    // failure -> renegotiation) as well as a runtime encoder failure clearing |h264_enabled_|.
    if (h264_supported && h264_enabled_)
        video_encoding_ = proto::video::ENCODING_H264;
    else if (vp8_supported)
        video_encoding_ = proto::video::ENCODING_VP8;
    else if (vp9_supported)
        video_encoding_ = proto::video::ENCODING_VP9;
    else
    {
        LOG(ERROR) << "No common video encoding among clients";
        return;
    }

    createVideoEncoder();
    if (!video_encoder_)
        return;

    if (!screen_capturer_ && !startCapturer())
    {
        // Screen capture needs the MediaProjection consent and a running foreground service; create()
        // fails until they are in place.
        LOG(ERROR) << "Screen capturer is not available";
        return;
    }

    // Input is injected through the accessibility service; it is a no-op until the user enables it.
    if (!input_injector_)
        input_injector_ = InputInjectorAndroid::create(this);

    video_encoder_->setKeyFrameRequired(true);
    onPreferredSizeChanged();
    sendScreenList();

    // Audio capture reuses the screen MediaProjection (obtained asynchronously), so it is started lazily
    // from onCaptureScreen once frames flow; here we only record whether a client wants it.
    audio_enabled_ = isAudioEnabled();

    if (floating_menu_bridge_)
        floating_menu_bridge_->showButton();

    capture_timer_->start(0);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onKeyFrameRequested()
{
    if (video_encoder_)
        video_encoder_->setKeyFrameRequired(true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onPreferredSizeChanged()
{
    // Encode at the largest size any client asked for, so none is upscaled on its side.
    QSize max_size;

    for (auto* client : std::as_const(clients_))
    {
        const QSize& size = client->preferredSize();
        if (size.width() * size.height() > max_size.width() * max_size.height())
            max_size = size;
    }

    preferred_size_ = max_size;
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onScreenListRequested()
{
    if (video_encoder_)
        video_encoder_->setKeyFrameRequired(true);
    sendScreenList();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClientFinished()
{
    DesktopClient* client = qobject_cast<DesktopClient*>(sender());
    if (!client)
        return;

    clients_.removeOne(client);
    client->deleteLater();

    audio_enabled_ = isAudioEnabled();
    if (!audio_enabled_ && audio_capturer_)
    {
        LOG(INFO) << "Stopping audio capture";
        audio_capturer_->disconnect();
        audio_capturer_.reset();
    }

    // The last desktop client left: stop capturing and release the projection (and its foreground
    // service). Capture restarts, re-requesting the consent, when a client connects again.
    if (clients_.isEmpty())
    {
        // Dispatch the button hide first: releasing the capturer blocks on the Android main thread, so
        // scheduling the hide beforehand keeps it from being held up behind that teardown.
        if (floating_menu_bridge_)
            floating_menu_bridge_->hideButton();

        capture_timer_->stop();
        screen_capturer_.reset();
        input_injector_.reset();
        video_encoder_.reset();
        source_size_ = QSize();
        frame_count_ = 0;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onCaptureScreen()
{
    if (!screen_capturer_)
        return;

    ScreenCapturer::Error error;
    const Frame* frame = screen_capturer_->captureFrame(&error);
    if (!frame)
    {
        const bool permanent = (error == ScreenCapturer::Error::PERMANENT);

        proto::video::HostToClient& message = outgoing_message_.newMessage<proto::video::HostToClient>();
        message.mutable_packet()->set_error_code(permanent ? proto::video::ERROR_CODE_PERMANENT
                                                           : proto::video::ERROR_CODE_TEMPORARY);
        const QByteArray& buffer = outgoing_message_.serialize<proto::video::HostToClient>();
        for (auto* client : std::as_const(clients_))
            client->onVideoData(buffer, false);

        // A permanent error (e.g. the user declined the capture consent) will not recover, so stop the
        // loop instead of resending the error every tick. A temporary one just waits for the first
        // frame (the projection is still being granted).
        if (!permanent)
            capture_timer_->start(kCaptureIntervalMs);
        return;
    }

    // Keep the injector's coordinate space in sync with the captured screen.
    if (input_injector_)
        input_injector_->setScreenInfo(screen_capturer_->desktopRect().size(), frame->topLeft());

    if (audio_enabled_ && !audio_capturer_)
    {
        LOG(INFO) << "Starting audio capture";

        audio_capturer_ = new AudioCapturerAndroid();
        connect(audio_capturer_.get(), &AudioCapturerAndroid::sig_audioPacket, this, &DesktopAgent::onAudioPacket);
        audio_capturer_->start();
    }

    encodeScreen(frame);
    capture_timer_->start(kCaptureIntervalMs);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectKeyEvent(const proto::input::KeyEvent& event)
{
    if (input_injector_)
        input_injector_->injectKeyEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectTextEvent(const proto::input::TextEvent& event)
{
    if (input_injector_)
        input_injector_->injectTextEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectMouseEvent(const proto::input::MouseEvent& event)
{
    if (!input_injector_)
        return;

    // The client's pointer position is in the scaled video space; map it back to the source screen.
    const double scale_x = scale_reducer_->scaleFactorX();
    const double scale_y = scale_reducer_->scaleFactorY();
    if (scale_x <= 0.0 || scale_y <= 0.0)
        return;

    proto::input::MouseEvent out_event;
    out_event.set_mask(event.mask());
    out_event.set_x(static_cast<int>(static_cast<double>(event.x()) * 100.0 / scale_x));
    out_event.set_y(static_cast<int>(static_cast<double>(event.y()) * 100.0 / scale_y));

    input_injector_->injectMouseEvent(out_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectTouchEvent(const proto::input::TouchEvent& event)
{
    if (input_injector_)
        input_injector_->injectTouchEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectClipboardEvent(const proto::clipboard::Event& event)
{
    // Only plain text is supported; queue it for the overlay button to apply on the next tap.
    if (event.mime_type() == Clipboard::kMimeTypeTextUtf8.toStdString() && floating_menu_bridge_)
        floating_menu_bridge_->setIncomingText(QString::fromStdString(event.data()));
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClipboardTextChanged(const QString& text)
{
    proto::clipboard::Event event;
    event.set_mime_type(Clipboard::kMimeTypeTextUtf8.toStdString());
    event.set_data(text.toStdString());

    proto::clipboard::HostToClient message;
    message.mutable_event()->CopyFrom(event);

    const QByteArray buffer = serialize(message);
    for (auto* client : std::as_const(clients_))
        client->onClipboardData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onAudioPacket(const QByteArray& packet)
{
    for (auto* client : std::as_const(clients_))
        client->onAudioData(packet);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::createVideoEncoder()
{
    video_encoder_ = VideoEncoder::create(video_encoding_);
    if (!video_encoder_)
        LOG(ERROR) << "Unable to create video encoder for encoding:" << video_encoding_;
}

//--------------------------------------------------------------------------------------------------
bool DesktopAgent::startCapturer()
{
    screen_capturer_ = ScreenCapturerAndroid::create(this);
    return screen_capturer_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
bool DesktopAgent::isAudioEnabled() const
{
    bool wanted = false;

    for (auto* client : std::as_const(clients_))
    {
        if (!client->isConfigured())
            continue;

        if (!client->isOpusSupported())
            return false;
        if (client->isAudioEnabled())
            wanted = true;
    }

    return wanted;
}


//--------------------------------------------------------------------------------------------------
void DesktopAgent::sendScreenList()
{
    if (!screen_capturer_)
        return;

    ScreenCapturer::ScreenList screen_list;
    if (!screen_capturer_->screenList(&screen_list))
    {
        LOG(ERROR) << "ScreenCapturer::screenList failed";
        return;
    }

    proto::screen::HostToClient message;
    proto::screen::ScreenList* list = message.mutable_screen_list();
    list->set_current_screen(screen_capturer_->currentScreen());

    for (const auto& screen_item : std::as_const(screen_list.screens))
    {
        proto::screen::Screen* screen = list->add_screen();
        screen->set_id(screen_item.id);
        screen->set_title(screen_item.title.toStdString());

        proto::screen::Point* position = screen->mutable_position();
        position->set_x(screen_item.position.x());
        position->set_y(screen_item.position.y());

        proto::screen::Size* resolution = screen->mutable_resolution();
        resolution->set_width(screen_item.resolution.width());
        resolution->set_height(screen_item.resolution.height());

        proto::screen::Point* dpi = screen->mutable_dpi();
        dpi->set_x(screen_item.dpi.x());
        dpi->set_y(screen_item.dpi.y());

        if (screen_item.is_primary)
            list->set_primary_screen(screen_item.id);
    }

    const QByteArray buffer = serialize(message);
    for (auto* client : std::as_const(clients_))
        client->onScreenListData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::encodeScreen(const Frame* frame)
{
    if (!frame || !video_encoder_)
        return;

    // Skip unchanged frames unless a key frame is pending.
    if (frame->constUpdatedRegion().isEmpty() && frame_count_ > 0 && !video_encoder_->isKeyFrameRequired())
        return;

    ++frame_count_;

    if (source_size_ != frame->size())
    {
        // A resolution change invalidates any preferred size.
        source_size_ = frame->size();
        preferred_size_ = QSize(0, 0);
    }

    QSize current_size = preferred_size_;

    // Never upscale past the source, and fall back to the source size when no preference is set.
    if (current_size.width() > source_size_.width() || current_size.height() > source_size_.height())
        current_size = source_size_;
    if (current_size.isEmpty())
        current_size = source_size_;

    const Frame* scaled_frame = scale_reducer_->scaleFrame(frame, current_size);
    if (!scaled_frame)
    {
        LOG(ERROR) << "No scaled frame";
        return;
    }

    proto::video::HostToClient& message = outgoing_message_.newMessage<proto::video::HostToClient>();
    proto::video::Packet* packet = message.mutable_packet();

    const VideoEncoder::Result result = video_encoder_->encode(scaled_frame, packet);
    if (result == VideoEncoder::Result::PERMANENT_ERROR)
    {
        // HW encoder cannot handle this frame size or driver state. Fall back to VP8.
        LOG(ERROR) << "Permanent encoder failure for" << video_encoding_ << "- falling back to VP8";
        h264_enabled_ = false;
        video_encoding_ = proto::video::ENCODING_VP8;
        createVideoEncoder();
        return;
    }

    if (result == VideoEncoder::Result::TEMPORARY_ERROR)
    {
        LOG(WARNING) << "Temporary encoder failure - skipping frame";
        return;
    }

    if (packet->has_format())
    {
        proto::video::PacketFormat* format = packet->mutable_format();
        format->set_capturer_type(static_cast<proto::video::ScreenCapturerType>(frame->capturerType()));

        proto::video::Size* screen_size = format->mutable_screen_size();
        screen_size->set_width(frame->size().width());
        screen_size->set_height(frame->size().height());
    }

    const bool is_key_frame = packet->flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME;

    const QByteArray& buffer = outgoing_message_.serialize<proto::video::HostToClient>();
    for (auto* client : std::as_const(clients_))
        client->onVideoData(buffer, is_key_frame);

    // Recycle the encode output buffer for the next frame.
    video_encoder_->setEncodeBuffer(std::move(*packet->mutable_data()));
}
