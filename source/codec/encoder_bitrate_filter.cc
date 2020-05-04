//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "codec/encoder_bitrate_filter.h"

#include <algorithm>
#include <cstdlib>

namespace codec {

namespace {

// Only update encoder bitrate when bandwidth changes by more than 33%. This value is chosen such
// that the codec is notified about significant changes in bandwidth, while ignoring bandwidth
// estimate noise. This is necessary because the encoder drops quality every time it's being
// reconfigured. When using VP8 encoder in realtime mode, the encoded frame size correlates very
// poorly with the target bitrate, so it's not necessary to set target bitrate to match bandwidth
// exactly. Send bitrate is controlled more precisely by adjusting time intervals between frames
// (i.e. FPS).
constexpr int kEncoderBitrateChangePercentage = 33;

// We use a WeightedSamples to analyze the bandwidth to avoid a sharp change to significantly
// impact the image quality. By using the weight factor as 0.95, the weight of a bandwidth estimate
// one second ago (30 frames before) drops to ~21.5%.
constexpr double kBandwidthWeightFactor = 0.95;

} // namespace

EncoderBitrateFilter::EncoderBitrateFilter(
    int minimum_bitrate_kbps_per_megapixel)
    : minimum_bitrate_kbps_per_megapixel_(minimum_bitrate_kbps_per_megapixel),
      bandwidth_kbps_(kBandwidthWeightFactor)
{
    // Nothing
}

EncoderBitrateFilter::~EncoderBitrateFilter() = default;

void EncoderBitrateFilter::setBandwidthEstimateKbps(int bandwidth_kbps)
{
    bandwidth_kbps_.record(bandwidth_kbps);

    int current_kbps = bandwidth_kbps_.weightedAverage();
    if (std::abs(current_kbps - bitrate_kbps_) >
        bitrate_kbps_ * kEncoderBitrateChangePercentage / 100)
    {
        bitrate_kbps_ = current_kbps;
    }
}

void EncoderBitrateFilter::setFrameSize(int width, int height)
{
    minimum_bitrate_kbps_ =
        std::max(static_cast<int64_t>(minimum_bitrate_kbps_per_megapixel_) *
                 width * height / 1000000,
                 static_cast<int64_t>(minimum_bitrate_kbps_));
}

int EncoderBitrateFilter::targetBitrateKbps() const
{
    return bitrate_kbps_;
}

} // namespace codec
