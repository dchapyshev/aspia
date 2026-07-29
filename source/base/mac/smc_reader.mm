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

#include "base/mac/smc_reader.h"

#include <QtGlobal>

#include <algorithm>
#include <cstring>

#import <IOKit/IOKitLib.h>

#include "base/logging.h"

namespace {

// Selector of the user client the controller is talked to through, and the commands it takes.
constexpr quint32 kUserClientType = 2;
constexpr quint8 kCommandReadBytes = 5;
constexpr quint8 kCommandReadIndex = 8;
constexpr quint8 kCommandReadKeyInfo = 9;

// Encodings a temperature sensor is reported in: a float on Apple Silicon, a fixed point number
// with eight fractional bits on Intel.
constexpr quint32 kTypeFloat = 'flt ';
constexpr quint32 kTypeFixedPoint = 'sp78';

// A reading outside of this range is not a temperature: sensors that have nothing to measure keep
// answering, and what they answer with is not always zero.
constexpr double kMinTemperature = 1.0;
constexpr double kMaxTemperature = 150.0;

// The controller is asked for the number of keys it has before they are enumerated, but the answer
// is not trusted to be sane.
constexpr quint32 kMaxKeyCount = 4096;

// Layout the user client expects. Every call takes and returns the same structure, the command
// field selects what it means.
struct SmcVersion
{
    quint8 major;
    quint8 minor;
    quint8 build;
    quint8 reserved;
    quint16 release;
};

struct SmcLimitData
{
    quint16 version;
    quint16 length;
    quint32 cpu_limit;
    quint32 gpu_limit;
    quint32 memory_limit;
};

struct SmcKeyInfo
{
    quint32 data_size;
    quint32 data_type;
    quint8 data_attributes;
};

struct SmcParam
{
    quint32 key;
    SmcVersion version;
    SmcLimitData limit_data;
    SmcKeyInfo key_info;
    quint8 result;
    quint8 status;
    quint8 command;
    quint32 data32;
    quint8 bytes[32];
};

//--------------------------------------------------------------------------------------------------
// Keys and the names of their types are four characters packed into an integer.
quint32 keyFromString(const char* name)
{
    return (static_cast<quint32>(name[0]) << 24) | (static_cast<quint32>(name[1]) << 16) |
           (static_cast<quint32>(name[2]) << 8) | static_cast<quint32>(name[3]);
}

//--------------------------------------------------------------------------------------------------
void stringFromKey(quint32 key, char* name)
{
    name[0] = static_cast<char>(key >> 24);
    name[1] = static_cast<char>(key >> 16);
    name[2] = static_cast<char>(key >> 8);
    name[3] = static_cast<char>(key);
    name[4] = 0;
}

//--------------------------------------------------------------------------------------------------
bool call(io_connect_t connection, const SmcParam& input, SmcParam* output)
{
    size_t output_size = sizeof(SmcParam);

    if (IOConnectCallStructMethod(connection, kUserClientType, &input, sizeof(input), output,
                                  &output_size) != KERN_SUCCESS)
    {
        return false;
    }

    return output->result == 0;
}

//--------------------------------------------------------------------------------------------------
bool readKey(io_connect_t connection, quint32 key, SmcKeyInfo* key_info, quint8* bytes)
{
    SmcParam input;
    SmcParam output;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));

    input.key = key;
    input.command = kCommandReadKeyInfo;

    if (!call(connection, input, &output))
        return false;

    *key_info = output.key_info;

    if (key_info->data_size > sizeof(output.bytes))
        return false;

    memset(&input, 0, sizeof(input));
    input.key = key;
    input.command = kCommandReadBytes;
    input.key_info.data_size = key_info->data_size;

    memset(&output, 0, sizeof(output));

    if (!call(connection, input, &output))
        return false;

    memcpy(bytes, output.bytes, sizeof(output.bytes));
    return true;
}

//--------------------------------------------------------------------------------------------------
bool keyAtIndex(io_connect_t connection, quint32 index, quint32* key)
{
    SmcParam input;
    SmcParam output;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));

    input.command = kCommandReadIndex;
    input.data32 = index;

    if (!call(connection, input, &output))
        return false;

    *key = output.key;
    return true;
}

//--------------------------------------------------------------------------------------------------
// Converts a reading to degrees Celsius. Returns 0 for a type the sensor is not expected to use.
double temperatureFromBytes(const SmcKeyInfo& key_info, const quint8* bytes)
{
    if (key_info.data_type == kTypeFloat && key_info.data_size == sizeof(float))
    {
        float value = 0.0f;
        memcpy(&value, bytes, sizeof(value));
        return static_cast<double>(value);
    }

    if (key_info.data_type == kTypeFixedPoint && key_info.data_size == sizeof(qint16))
    {
        const qint16 value = static_cast<qint16>((bytes[0] << 8) | bytes[1]);
        return value / 256.0;
    }

    return 0.0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
#if defined(Q_PROCESSOR_ARM_64)
const char SmcReader::kProcessorTemperatureKeys[] = "Tp";
#else
const char SmcReader::kProcessorTemperatureKeys[] = "TC";
#endif

//--------------------------------------------------------------------------------------------------
// static
quint32 SmcReader::maxTemperature(const char* key_prefix)
{
    io_service_t service =
        IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSMC"));
    if (!service)
    {
        LOG(ERROR) << "System management controller not found";
        return 0;
    }

    io_connect_t connection = IO_OBJECT_NULL;
    const kern_return_t status = IOServiceOpen(service, mach_task_self(), 0, &connection);

    IOObjectRelease(service);

    if (status != KERN_SUCCESS)
    {
        LOG(ERROR) << "IOServiceOpen failed:" << status;
        return 0;
    }

    SmcKeyInfo key_info;
    quint8 bytes[sizeof(SmcParam::bytes)];

    quint32 key_count = 0;
    if (readKey(connection, keyFromString("#KEY"), &key_info, bytes) && key_info.data_size == 4)
    {
        key_count = (static_cast<quint32>(bytes[0]) << 24) | (static_cast<quint32>(bytes[1]) << 16) |
                    (static_cast<quint32>(bytes[2]) << 8) | static_cast<quint32>(bytes[3]);
    }

    const size_t prefix_length = strlen(key_prefix);
    double max_temperature = 0.0;

    for (quint32 index = 0; index < key_count && index < kMaxKeyCount; ++index)
    {
        quint32 key = 0;
        if (!keyAtIndex(connection, index, &key))
            continue;

        char name[5];
        stringFromKey(key, name);

        if (strncmp(name, key_prefix, prefix_length) != 0)
            continue;

        if (!readKey(connection, key, &key_info, bytes))
            continue;

        const double temperature = temperatureFromBytes(key_info, bytes);
        if (temperature < kMinTemperature || temperature > kMaxTemperature)
            continue;

        max_temperature = std::max(max_temperature, temperature);
    }

    IOServiceClose(connection);

    return static_cast<quint32>(max_temperature * 10.0);
}
