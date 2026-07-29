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

#ifndef BASE_MAC_SMC_READER_H
#define BASE_MAC_SMC_READER_H

#include <QtClassHelperMacros>
#include <QtTypes>

// Reads the sensors of the system management controller. The keys the controller answers to differ
// between models, so sensors are looked up by the prefix of their name rather than by a fixed list.
class SmcReader
{
public:
    // Prefix the keys of the temperature sensors of the processor cores start with. The controller
    // of an Apple Silicon machine names them differently than the one of an Intel machine.
    static const char kProcessorTemperatureKeys[];

    // Highest reading of the temperature sensors whose key starts with |key_prefix|, in tenths of a
    // degree Celsius. Returns 0 when the controller reports no such sensor.
    static quint32 maxTemperature(const char* key_prefix);

private:
    Q_DISABLE_COPY_MOVE(SmcReader)
};

#endif // BASE_MAC_SMC_READER_H
