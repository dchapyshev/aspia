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

#ifndef BASE_CODEC_VECTOR_MATH_H
#define BASE_CODEC_VECTOR_MATH_H

#include <utility>

namespace base {

// Required alignment for inputs and outputs to all vector math functions
enum { kRequiredAlignment = 16 };

// Multiply each element of |src| (up to |len|) by |scale| and add to |dest|.
// |src| and |dest| must be aligned by kRequiredAlignment.
void FMAC(const float src[], float scale, int len, float dest[]);

// Multiply each element of |src| by |scale| and store in |dest|.  |src| and
// |dest| must be aligned by kRequiredAlignment.
void FMUL(const float src[], float scale, int len, float dest[]);

// Computes the exponentially-weighted moving average power of a signal by
// iterating the recurrence:
//
//   y[-1] = initial_value
//   y[n] = smoothing_factor * src[n]^2 + (1-smoothing_factor) * y[n-1]
//
// Returns the final average power and the maximum squared element value.
std::pair<float, float> EWMAAndMaxPower(
    float initial_value, const float src[], int len, float smoothing_factor);

void crossfade(const float src[], int len, float dest[]);

} // namespace base

#endif // BASE_CODEC_VECTOR_MATH_H
