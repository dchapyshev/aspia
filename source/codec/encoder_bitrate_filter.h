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

#ifndef CODEC__ENCODER_BITRATE_FILTER_H
#define CODEC__ENCODER_BITRATE_FILTER_H

#include "codec/weighted_samples.h"

namespace codec {

// Receives bandwidth estimations, frame size, etc and decide the best bitrate for encoder.
class EncoderBitrateFilter final
{
public:
    explicit EncoderBitrateFilter(int minimum_bitrate_kbps_per_megapixel);
    ~EncoderBitrateFilter();

    void setBandwidthEstimateKbps(int bandwidth_kbps);
    void setFrameSize(int width, int height);
    int targetBitrateKbps() const;

private:
    const int minimum_bitrate_kbps_per_megapixel_;
    // This is the minimum number to avoid returning unreasonable value from targetBitrateKbps().
    // It roughly equals to the minimum bitrate of a 780x512 screen for VP8, or 1024x558 screen for
    // H264.
    int minimum_bitrate_kbps_ = 1000;
    WeightedSamples bandwidth_kbps_;
    int bitrate_kbps_ = minimum_bitrate_kbps_;
};

} // namespace codec

#endif // CODEC__ENCODER_BITRATE_FILTER_H
