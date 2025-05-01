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

#include "base/edid.h"

#include "base/bitset.h"
#include "base/endian_util.h"
#include "base/logging.h"

#include <fmt/format.h>

#include <cstring>

namespace base {

namespace {

const size_t kMinEdidSize = 0x80; // 128 bytes
const quint64 kEdidHeader = 0x00FFFFFFFFFFFF00;
const uint8_t kMinWeekOfManufacture = 1;
const uint8_t kMaxWeekOfManufacture = 53;
const int kUnknownDescriptor = -1;
const int kDetailedTimingDescriptor = -2;

struct Manufacturers
{
    const char* signature;
    const char* name;
} kManufacturers[] =
{
    { "ABO", "Acer" },
    { "ACI", "Asus" },
    { "ACR", "Acer" },
    { "AIC", "AG" },
    { "AOC", "AOC" },
    { "API", "Acer" },
    { "AUO", "AU Optronics Corp." },
    { "BNQ", "BenQ" },
    { "CHD", "ChangHong Electric Co.,Ltd." },
    { "CPQ", "Compaq" },
    { "CTC", "CTC" },
    { "CTX", "CTX" },
    { "DEL", "Dell" },
    { "DON", "Denon" },
    { "DNY", "Disney" },
    { "EIZ", "EIZO" },
    { "ENC", "ENC" },
    { "EPI", "EPI" },
    { "GSM", "LG" },
    { "GWY", "Gateway" },
    { "HCG", "Harman Kardon" },
    { "HEC", "Hisense Electric Co., Ltd." },
    { "HSL", "Hansol" },
    { "HTC", "Hitachi" },
    { "HWP", "HP" },
    { "IBM", "IBM" },
    { "IFS", "In Focus Systems Inc" },
    { "IVM", "Iiyama" },
    { "IQT", "ImageQuest" },
    { "JEN", "Acer" },
    { "JVC", "JVC" },
    { "YMH", "Yamaha" },
    { "MAX", "Belinea" },
    { "MEA", "Diamond" },
    { "MED", "Medion" },
    { "MEI", "Panasonic" },
    { "MEL", "Mitsubishi" },
    { "MJI", "Marantz" },
    { "NEC", "NEC" },
    { "NOK", "Nokia" },
    { "PHL", "Philips" },
    { "PIO", "Pioneer" },
    { "PRI", "Prima" },
    { "PYX", "PYX" },
    { "PTS", "MAG" },
    { "LEN", "Lenovo" },
    { "LGE", "LG" },
    { "LGD", "Samsung" },
    { "LTN", "Acer" },
    { "ONK", "Onkyo" },
    { "SAM", "Samsung" },
    { "SHP", "Sharp" },
    { "SNY", "Sony" },
    { "STN", "Samtron" },
    { "TAT", "Tatung" },
    { "TPV", "TPV" },
    { "TSB", "Toshiba" },
    { "VIZ", "Vizio" },
    { "VSC", "ViewSonic" }
};

} // namespace

//--------------------------------------------------------------------------------------------------
static int GetDataType(uint8_t* descriptor)
{
    const uint8_t kEdidV1DescriptorFlag[] = { 0x00, 0x00 };

    if (memcmp(descriptor, kEdidV1DescriptorFlag, 2) == 0)
    {
        if (descriptor[2] != 0)
            return kUnknownDescriptor;

        return descriptor[3];
    }

    return kDetailedTimingDescriptor;
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<Edid> Edid::create(std::unique_ptr<uint8_t[]> data, size_t data_size)
{
    static_assert(sizeof(Data) == kMinEdidSize);

    if (data && data_size >= sizeof(Data))
    {
        Data* edid = reinterpret_cast<Data*>(data.get());

        if (edid->header == kEdidHeader)
        {
            uint8_t checksum = 0;

            for (size_t index = 0; index < kMinEdidSize; ++index)
                checksum += data[index];

            // The 1-byte sum of all 128 bytes in this EDID block shall equal zero.
            if (!checksum)
            {
                return std::unique_ptr<Edid>(new Edid(std::move(data), data_size));
            }

            LOG(LS_ERROR) << "Invalid EDID checksum: " << checksum;
        }
        else
        {
            LOG(LS_ERROR) << "Invalid EDID header: " << edid->header;
        }
    }
    else
    {
        LOG(LS_ERROR) << "Invalid EDID data";
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
Edid::Edid(std::unique_ptr<uint8_t[]> data, size_t data_size)
    : data_(std::move(data)),
      data_size_(data_size)
{
    edid_ = reinterpret_cast<Data*>(data_.get());
}

//--------------------------------------------------------------------------------------------------
int Edid::weekOfManufacture() const
{
    uint8_t week = edid_->week_of_manufacture;

    if (week < kMinWeekOfManufacture || week > kMaxWeekOfManufacture)
    {
        LOG(LS_ERROR) << "Wrong week field value: " << week;
        return 0;
    }

    return week;
}

//--------------------------------------------------------------------------------------------------
int Edid::yearOfManufacture() const
{
    // The Year of Manufacture field is used to represent the year of the monitor’s manufacture.
    // The value that is stored is an offset from the year 1990 as derived from the following
    // equation:
    // Value stored = (Year of manufacture - 1990)

    return 1990 + edid_->year_of_manufacture;
}

//--------------------------------------------------------------------------------------------------
int Edid::edidVersion() const
{
    return edid_->structure_version;
}

//--------------------------------------------------------------------------------------------------
int Edid::edidRevision() const
{
    return edid_->structure_revision;
}

//--------------------------------------------------------------------------------------------------
int Edid::maxHorizontalImageSize() const
{
    return edid_->max_horizontal_image_size;
}

//--------------------------------------------------------------------------------------------------
int Edid::maxVerticalImageSize() const
{
    return edid_->max_vertical_image_size;
}

//--------------------------------------------------------------------------------------------------
int Edid::horizontalResolution() const
{
    DetailedTimingDescriptor* descriptor =
        reinterpret_cast<DetailedTimingDescriptor*>(getDescriptor(kDetailedTimingDescriptor));

    if (!descriptor)
        return 0;

    uint32_t lo = static_cast<uint32_t>(descriptor->horizontal_active);
    uint32_t hi = ((0xF0 & static_cast<uint32_t>(descriptor->horizontal_active_blanking)) >> 4);

    return static_cast<int>((hi << 8) | lo);
}

//--------------------------------------------------------------------------------------------------
int Edid::verticalResolution() const
{
    DetailedTimingDescriptor* descriptor =
        reinterpret_cast<DetailedTimingDescriptor*>(getDescriptor(kDetailedTimingDescriptor));

    if (!descriptor)
        return 0;

    uint32_t lo = static_cast<uint32_t>(descriptor->vertical_active);
    uint32_t hi = ((0xF0 & static_cast<uint32_t>(descriptor->vertical_active_blanking)) >> 4);

    return static_cast<int>((hi << 8) | lo);
}

//--------------------------------------------------------------------------------------------------
double Edid::gamma() const
{
    const uint8_t gamma = edid_->gamma;

    if (gamma == 0xFF)
        return 0.0;

    return (static_cast<double>(gamma) / 100.0) + 1.0;
}

//--------------------------------------------------------------------------------------------------
uint8_t Edid::featureSupport() const
{
    return edid_->feature_support;
}

//--------------------------------------------------------------------------------------------------
std::string Edid::getManufacturerSignature() const
{
    BitSet<quint16> id = EndianUtil::byteSwap(edid_->id_manufacturer_name);

    // Bits 14:10 : first letter (01h = 'A', 02h = 'B', etc.).
    // Bits 9:5 : second letter.
    // Bits 4:0 : third letter.
    char signature[4];
    signature[0] = static_cast<char>(id.range(10, 14)) + 'A' - 1;
    signature[1] = static_cast<char>(id.range(5, 9)) + 'A' - 1;
    signature[2] = static_cast<char>(id.range(0, 4)) + 'A' - 1;
    signature[3] = 0;

    return signature;
}

//--------------------------------------------------------------------------------------------------
std::string Edid::monitorId() const
{
    return fmt::format("{}{:x}", getManufacturerSignature(), edid_->id_product_code);
}

//--------------------------------------------------------------------------------------------------
std::string Edid::serialNumber() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(
            getDescriptor(DATA_TYPE_TAG_MONITOR_SERIAL_NUMBER_ASCII));

    if (descriptor)
    {
        const size_t kMaxMonitorSNLength = 13;

        char sn[kMaxMonitorSNLength];

        for (size_t i = 0; i < kMaxMonitorSNLength; ++i)
        {
            if (descriptor->descriptor_data[i] == 0x0A)
            {
                sn[i] = 0;
                return sn;
            }

            sn[i] = static_cast<char>(descriptor->descriptor_data[i]);
        }
    }

    return std::string();
}

//--------------------------------------------------------------------------------------------------
uint8_t* Edid::getDescriptor(int type) const
{
    size_t count = sizeof(Data::detailed_timing_description) /
        sizeof(Data::detailed_timing_description[0]);

    for (size_t index = 0; index < count; ++index)
    {
        uint8_t* descriptor = &edid_->detailed_timing_description[index][0];

        if (GetDataType(descriptor) == type)
            return descriptor;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
std::string Edid::manufacturerName() const
{
    std::string signature = getManufacturerSignature();

    for (size_t i = 0; i < std::size(kManufacturers); ++i)
    {
        if (signature == kManufacturers[i].signature)
            return kManufacturers[i].name;
    }

    return std::string();
}

//--------------------------------------------------------------------------------------------------
std::string Edid::monitorName() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MONITOR_NAME_ASCII));

    if (descriptor)
    {
        const size_t kMaxMonitorNameLength = 13;

        char name[kMaxMonitorNameLength];

        for (size_t i = 0; i < kMaxMonitorNameLength; ++i)
        {
            if (descriptor->descriptor_data[i] == 0x0A)
            {
                name[i] = 0;
                return name;
            }

            name[i] = static_cast<char>(descriptor->descriptor_data[i]);
        }
    }

    return std::string();
}

//--------------------------------------------------------------------------------------------------
int Edid::minVerticalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[0];
}

//--------------------------------------------------------------------------------------------------
int Edid::maxVerticalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[1];
}

//--------------------------------------------------------------------------------------------------
int Edid::minHorizontalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[2];
}

//--------------------------------------------------------------------------------------------------
int Edid::maxHorizontalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[3];
}

//--------------------------------------------------------------------------------------------------
int Edid::maxSupportedPixelClock() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[4] * 10;
}

//--------------------------------------------------------------------------------------------------
double Edid::pixelClock() const
{
    DetailedTimingDescriptor* descriptor =
        reinterpret_cast<DetailedTimingDescriptor*>(getDescriptor(kDetailedTimingDescriptor));

    if (!descriptor)
        return 0;

    return double(descriptor->pixel_clock) / 100.0;
}

//--------------------------------------------------------------------------------------------------
Edid::InputSignalType Edid::inputSignalType() const
{
    if (edid_->video_input_definition & 0x80)
        return INPUT_SIGNAL_TYPE_DIGITAL;

    return INPUT_SIGNAL_TYPE_ANALOG;
}

//--------------------------------------------------------------------------------------------------
uint8_t Edid::estabilishedTimings1() const
{
    return edid_->established_timings[0];
}

//--------------------------------------------------------------------------------------------------
uint8_t Edid::estabilishedTimings2() const
{
    return edid_->established_timings[1];
}

//--------------------------------------------------------------------------------------------------
uint8_t Edid::manufacturersTimings() const
{
    return edid_->manufacturers_reserved_timings;
}

//--------------------------------------------------------------------------------------------------
int Edid::standardTimingsCount() const
{
    size_t count = sizeof(Data::standard_timing_identification) /
        sizeof(Data::standard_timing_identification[0]);
    return static_cast<int>(count);
}

//--------------------------------------------------------------------------------------------------
bool Edid::standardTimings(int index, int* width, int* height, int* frequency)
{
    uint8_t byte1 = edid_->standard_timing_identification[index][0];
    uint8_t byte2 = edid_->standard_timing_identification[index][1];

    if (byte1 == 0x01 && byte2 == 0x01)
        return false;

    if (byte1 == 0x00)
        return false;

    int ratio_w;
    int ratio_h;

    *width = (byte1 + 31) * 8;

    switch ((byte2 >> 6) & 0x03)
    {
        case 0x00:
        {
            if (edid_->structure_revision == 3)
            {
                ratio_w = 16;
                ratio_h = 10;
            }
            else
            {
                ratio_w = 1;
                ratio_h = 1;
            }
        }
        break;

        case 0x01:
        {
            ratio_w = 4;
            ratio_h = 3;
        }
        break;

        case 0x02:
        {
            ratio_w = 5;
            ratio_h = 4;
        }
        break;

        case 0x03:
        {
            ratio_w = 16;
            ratio_h = 9;
        }
        break;

        default:
            return false;
    }

    *height = *width * ratio_h / ratio_w;
    *frequency = 60 + (byte2 & 0x3F);

    return true;
}

} // namespace base
