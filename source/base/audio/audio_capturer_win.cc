//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/audio/audio_capturer_win.h"

#include "base/logging.h"
#include "base/audio/win/default_audio_device_change_detector.h"

#include <algorithm>
#include <utility>

#include <avrt.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <objbase.h>
#include <qt_windows.h>
#include <comdef.h>

namespace {

const int kBytesPerSample = 2;
const int kBitsPerSample = kBytesPerSample * 8;

// Conversion factor from 100ns to 1ms.
const int k100nsPerMillisecond = 10000;

// Tolerance for catching packets of silence. If all samples have absolute
// value less than this threshold, the packet will be counted as a packet of
// silence. A value of 2 was chosen, because Windows can give samples of 1 and
// -1, even when no audio is playing.
const int kSilenceThreshold = 2;

// Lower bound for timer intervals, in milliseconds.
const std::chrono::milliseconds kMinTimerInterval { 30 };

// Upper bound for the timer precision error, in milliseconds.
// Timers are supposed to be accurate to 20ms, so we use 30ms to be safe.
const int kMaxExpectedTimerLag = 30;

}  // namespace

namespace base {

//--------------------------------------------------------------------------------------------------
AudioCapturerWin::AudioCapturerWin(QObject* parent)
    : AudioCapturer(parent),
      sampling_rate_(proto::AudioPacket::SAMPLING_RATE_INVALID),
      capture_timer_(new QTimer(this)),
      volume_filter_(kSilenceThreshold),
      last_capture_error_(S_OK)
{
    LOG(LS_INFO) << "Ctor";

    connect(capture_timer_, &QTimer::timeout, this, &AudioCapturerWin::onCaptureTimeout);
}

//--------------------------------------------------------------------------------------------------
AudioCapturerWin::~AudioCapturerWin()
{
    LOG(LS_INFO) << "Dtor";
    deinitialize();
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerWin::start(const PacketCapturedCallback& callback)
{
    LOG(LS_INFO) << "Starting audio capturer";
    callback_ = callback;

    if (!initialize())
    {
        LOG(LS_ERROR) << "initialize failed";
        return false;
    }

    // Initialize the capture timer and start capturing. Note, this timer won't be reset or
    // restarted in resetAndInitialize() function. Which means we expect the audio_device_period_
    // is a system wide configuration, it would not be changed with the default audio device.
    capture_timer_->start(audio_device_period_);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerWin::resetAndInitialize()
{
    deinitialize();
    if (!initialize())
    {
        LOG(LS_ERROR) << "initialize failed";
        deinitialize();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerWin::deinitialize()
{
    LOG(LS_INFO) << "Deinitialize audio capturer";

    wave_format_ex_.reset(nullptr);
    default_device_detector_.reset();
    audio_capture_client_.Reset();
    if (audio_client_)
        audio_client_->Stop();
    audio_client_.Reset();
    mm_device_.Reset();
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerWin::initialize()
{
    LOG(LS_INFO) << "Audio capturer initializing";
    DCHECK(!audio_capture_client_.Get());
    DCHECK(!audio_client_.Get());
    DCHECK(!mm_device_.Get());
    DCHECK(static_cast<PWAVEFORMATEX>(wave_format_ex_) == nullptr);

    _com_error hr = S_OK;
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> mm_device_enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&mm_device_enumerator));
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "Failed to create IMMDeviceEnumerator. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();
        return false;
    }

    default_device_detector_.reset(new DefaultAudioDeviceChangeDetector(mm_device_enumerator));

    // Get the audio endpoint.
    hr = mm_device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, mm_device_.GetAddressOf());
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "Failed to get IMMDevice. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();
        return false;
    }

    // Get an audio client.
    hr = mm_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audio_client_);
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "Failed to get an IAudioClient. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();
        return false;
    }

    REFERENCE_TIME device_period;
    hr = audio_client_->GetDevicePeriod(&device_period, nullptr);
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "IAudioClient::GetDevicePeriod failed. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();
        return false;
    }

    // We round up, if |device_period| / |k100nsPerMillisecond| is not a whole number.
    std::chrono::milliseconds device_period_in_milliseconds(
        1 + ((device_period - 1) / k100nsPerMillisecond));

    audio_device_period_ = std::max(device_period_in_milliseconds, kMinTimerInterval);

    // Get the wave format.
    hr = audio_client_->GetMixFormat(&wave_format_ex_);
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "Failed to get WAVEFORMATEX. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();
        return false;
    }

    if (wave_format_ex_->wFormatTag != WAVE_FORMAT_IEEE_FLOAT &&
        wave_format_ex_->wFormatTag != WAVE_FORMAT_PCM &&
        wave_format_ex_->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
    {
        LOG(LS_ERROR) << "Failed to force 16-bit PCM";
        return false;
    }

    if (!AudioCapturer::isValidSampleRate(wave_format_ex_->nSamplesPerSec))
    {
        LOG(LS_ERROR) << "Host sampling rate is neither 44.1 kHz nor 48 kHz. "
                      << wave_format_ex_->nSamplesPerSec;
        return false;
    }

    // We support from mono to 7.1. This check should be consistent with AudioPacket::Channels.
    if (wave_format_ex_->nChannels > 8 || wave_format_ex_->nChannels <= 0)
    {
        LOG(LS_ERROR) << "Unsupported channels " << wave_format_ex_->nChannels;
        return false;
    }

    LOG(LS_INFO) << "Audio device period: " << audio_device_period_.count();
    LOG(LS_INFO) << "Audio device sample rate: " << wave_format_ex_->nSamplesPerSec;
    LOG(LS_INFO) << "Audio device channels: " << wave_format_ex_->nChannels;

    sampling_rate_ = static_cast<proto::AudioPacket::SamplingRate>(
        wave_format_ex_->nSamplesPerSec);

    wave_format_ex_->wBitsPerSample = kBitsPerSample;
    wave_format_ex_->nBlockAlign = wave_format_ex_->nChannels * kBytesPerSample;
    wave_format_ex_->nAvgBytesPerSec = sampling_rate_ * wave_format_ex_->nBlockAlign;

    if (wave_format_ex_->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        PWAVEFORMATEXTENSIBLE wave_format_extensible = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(
            static_cast<WAVEFORMATEX*>(wave_format_ex_));
        if (!IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, wave_format_extensible->SubFormat) &&
            !IsEqualGUID(KSDATAFORMAT_SUBTYPE_PCM, wave_format_extensible->SubFormat))
        {
            LOG(LS_ERROR) << "Failed to force 16-bit samples";
            return false;
        }

        wave_format_extensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        wave_format_extensible->Samples.wValidBitsPerSample = kBitsPerSample;
    }
    else
    {
        wave_format_ex_->wFormatTag = WAVE_FORMAT_PCM;
    }

    // Initialize the IAudioClient.
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        (kMaxExpectedTimerLag + audio_device_period_.count()) * k100nsPerMillisecond,
        0,
        wave_format_ex_,
        nullptr);
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "Failed to initialize IAudioClient. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();

        if (hr.Error() == E_INVALIDARG)
        {
            LOG(LS_ERROR) << "Audio capture may be blocked by your antivirus. Try disabling your "
                             "antivirus or adding the application to the whitelist.";
        }
        return false;
    }

    // Get an IAudioCaptureClient.
    hr = audio_client_->GetService(IID_PPV_ARGS(&audio_capture_client_));
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "Failed to get an IAudioCaptureClient. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();
        return false;
    }

    // Start the IAudioClient.
    hr = audio_client_->Start();
    if (FAILED(hr.Error()))
    {
        LOG(LS_ERROR) << "Failed to start IAudioClient. Error " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error();
        return false;
    }

    volume_filter_.activateBy(mm_device_.Get());
    volume_filter_.initialize(sampling_rate_, wave_format_ex_->nChannels);

    LOG(LS_INFO) << "Audio capturer initialized";
    return true;
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerWin::isInitialized() const
{
    // All Com components should be initialized / deinitialized together.
    return !!audio_client_;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerWin::doCapture()
{
    DCHECK(AudioCapturer::isValidSampleRate(sampling_rate_));

    if (!isInitialized() || default_device_detector_->getAndReset())
    {
        if (!resetAndInitialize())
        {
            // Initialization failed, we should wait for next doCapture call.
            return;
        }
    }

    // Fetch all packets from the audio capture endpoint buffer.
    _com_error hr = S_OK;
    while (true)
    {
        UINT32 next_packet_size;
        hr = audio_capture_client_->GetNextPacketSize(&next_packet_size);
        if (FAILED(hr.Error()))
            break;

        if (next_packet_size <= 0)
            return;

        BYTE* data;
        UINT32 frames;
        DWORD flags;
        hr = audio_capture_client_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr.Error()))
            break;

        if (volume_filter_.apply(reinterpret_cast<qint16*>(data), frames))
        {
            std::unique_ptr<proto::AudioPacket> packet(new proto::AudioPacket());
            packet->add_data(data, frames * wave_format_ex_->nBlockAlign);
            packet->set_encoding(proto::AUDIO_ENCODING_RAW);
            packet->set_sampling_rate(sampling_rate_);
            packet->set_bytes_per_sample(proto::AudioPacket::BYTES_PER_SAMPLE_2);
            // Only the count of channels is taken into account now, we should also
            // consider dwChannelMask.
            // TODO(zijiehe): Convert dwChannelMask to layout and pass it to
            // AudioPump. So the stream can be downmixed properly with both number and
            // layouts of speakers.
            packet->set_channels(static_cast<proto::AudioPacket::Channels>(
                wave_format_ex_->nChannels));

            callback_(std::move(packet));
        }

        hr = audio_capture_client_->ReleaseBuffer(frames);
        if (FAILED(hr.Error()))
            break;
    }

    // There is nothing to capture if the audio endpoint device has been unplugged or disabled.
    if (hr.Error() == AUDCLNT_E_DEVICE_INVALIDATED)
        return;

    // Avoid reporting the same error multiple times.
    if (FAILED(hr.Error()) && hr.Error() != last_capture_error_)
    {
        last_capture_error_ = hr.Error();
        LOG(LS_ERROR) << "Failed to capture an audio packet: " << hr.ErrorMessage()
                      << " with code " << Qt::hex << hr.Error() << ".";
    }
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerWin::onCaptureTimeout()
{
    doCapture();
    capture_timer_->start(audio_device_period_);
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturer::isSupported()
{
    return true;
}

} // namespace base
