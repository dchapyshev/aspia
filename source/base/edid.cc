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

#include "base/edid.h"

#include <QtEndian>

#include "base/bitset.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {

const size_t kMinEdidSize = 0x80; // 128 bytes
const quint64 kEdidHeader = 0x00FFFFFFFFFFFF00;
const quint8 kMinWeekOfManufacture = 1;
const quint8 kMaxWeekOfManufacture = 53;
const int kUnknownDescriptor = -1;
const int kDetailedTimingDescriptor = -2;

struct Manufacturers
{
    const char* signature;
    const char* name;
} kManufacturers[] =
{
    { "ACI", "Asus" },
    { "ACR", "Acer" },
    { "ADI", "ADI Systems" },
    { "AIC", "AG Neovo" },
    { "ANX", "Acer" },
    { "AOA", "AOpen" },
    { "AOC", "AOC" },
    { "APP", "Apple" },
    { "AST", "AST Research" },
    { "ASU", "Asus" },
    { "AUO", "AU Optronics" },
    { "AUS", "Asus" },
    { "BCD", "Barco" },
    { "BDS", "Barco" },
    { "BEK", "Beko" },
    { "BGB", "Barco" },
    { "BNQ", "BenQ" },
    { "BOE", "BOE" },
    { "BPS", "Barco" },
    { "CDG", "Christie Digital" },
    { "CHD", "ChangHong" },
    { "CHE", "Acer" },
    { "CHG", "ChangHong" },
    { "CHO", "ChangHong" },
    { "CMN", "Chi Mei Innolux" },
    { "CMO", "Chi Mei Optoelectronics" },
    { "CND", "MSI" },
    { "CPL", "Compal" },
    { "CPQ", "Compaq" },
    { "CRM", "Corsair" },
    { "CSW", "China Star Optoelectronics" },
    { "CTC", "CTC" },
    { "CTX", "CTX" },
    { "DEL", "Dell" },
    { "DLL", "Dell" },
    { "DNY", "Disney" },
    { "DON", "Denon" },
    { "DPC", "Delta Electronics" },
    { "DPL", "Digital Projection" },
    { "DTO", "Thomson" },
    { "DWE", "Daewoo" },
    { "ECS", "EliteGroup" },
    { "EGD", "EIZO" },
    { "EIZ", "EIZO" },
    { "ELO", "Elo Touch" },
    { "ELS", "ELSA" },
    { "ENC", "EIZO" },
    { "EPI", "Envision" },
    { "ERS", "EIZO" },
    { "ETG", "EIZO" },
    { "FCM", "Funai" },
    { "FDT", "Fujitsu" },
    { "FNI", "Funai" },
    { "FUJ", "Fujitsu" },
    { "FUS", "Fujitsu Siemens" },
    { "GBT", "Gigabyte" },
    { "GGL", "Google" },
    { "GSM", "LG" },
    { "GWY", "Gateway" },
    { "HCE", "Hitachi" },
    { "HCG", "Harman Kardon" },
    { "HEC", "Hisense" },
    { "HEI", "Hyundai" },
    { "HIT", "Hitachi" },
    { "HKC", "HKC" },
    { "HMX", "HUMAX" },
    { "HNM", "Honor" },
    { "HPC", "HP" },
    { "HPD", "HP" },
    { "HPE", "HP Enterprise" },
    { "HPN", "HP" },
    { "HPQ", "HP" },
    { "HRE", "Haier" },
    { "HSD", "HannStar" },
    { "HSL", "Hansol" },
    { "HSP", "HannStar" },
    { "HTC", "Hitachi" },
    { "HWP", "HP" },
    { "HWV", "Huawei" },
    { "IBM", "IBM" },
    { "IDT", "International Display Technology" },
    { "INL", "InnoLux" },
    { "IOD", "I-O Data" },
    { "IQT", "ImageQuest" },
    { "ITE", "ITE Tech" },
    { "IVM", "Iiyama" },
    { "IVO", "InfoVision" },
    { "JDI", "Japan Display" },
    { "JVC", "JVC" },
    { "KDS", "KDS USA" },
    { "LCD", "Toshiba Matsushita Display" },
    { "LEN", "Lenovo" },
    { "LGD", "LG Display" },
    { "LGE", "LG" },
    { "LIN", "Lenovo" },
    { "LNV", "Lenovo" },
    { "LOE", "Loewe" },
    { "LPL", "LG Philips" },
    { "MAG", "MAG InnoVision" },
    { "MAT", "Panasonic" },
    { "MAX", "Belinea" },
    { "MCE", "Metz" },
    { "MDO", "Panasonic" },
    { "MEA", "Diamond" },
    { "MED", "Medion" },
    { "MEE", "Mitsubishi" },
    { "MEI", "Panasonic" },
    { "MEL", "Mitsubishi" },
    { "MJI", "Marantz" },
    { "MSG", "MSI" },
    { "MSH", "Microsoft" },
    { "MSI", "MSI" },
    { "MTT", "Moore Threads" },
    { "MVD", "Microvitec" },
    { "MXD", "MaxData" },
    { "NAN", "Nanao" },
    { "NEC", "NEC" },
    { "NMV", "NEC-Mitsubishi" },
    { "NOK", "Nokia" },
    { "NVD", "Nvidia" },
    { "ONK", "Onkyo" },
    { "OPP", "OPPO" },
    { "OVR", "Oculus" },
    { "PBL", "Packard Bell" },
    { "PBN", "Packard Bell" },
    { "PGS", "Princeton Graphic Systems" },
    { "PHL", "Philips" },
    { "PIO", "Pioneer" },
    { "PNR", "Planar" },
    { "PRI", "Prima" },
    { "PRT", "Parade Technologies" },
    { "PTS", "MAG" },
    { "PVG", "Proview" },
    { "PXO", "Pixio" },
    { "PYX", "PYX" },
    { "QCI", "Quanta" },
    { "QDS", "Quanta Display" },
    { "RTL", "Realtek" },
    { "RZR", "Razer" },
    { "SAM", "Samsung" },
    { "SAN", "Sanyo" },
    { "SDC", "Samsung Display" },
    { "SDI", "Samtron" },
    { "SEC", "Seiko Epson" },
    { "SEM", "Samsung" },
    { "SHP", "Sharp" },
    { "SII", "Silicon Image" },
    { "SKG", "KTC" },
    { "SKW", "Skyworth" },
    { "SNI", "Siemens" },
    { "SNY", "Sony" },
    { "SON", "Sony" },
    { "SPO", "Sampo" },
    { "SPT", "Sceptre" },
    { "SSE", "Samsung" },
    { "STN", "Samsung" },
    { "SVI", "Sun Microsystems" },
    { "SYL", "Sylvania" },
    { "TAI", "Toshiba" },
    { "TAT", "Tatung" },
    { "TCL", "TCL" },
    { "TCR", "Thomson" },
    { "TCS", "Tatung" },
    { "TMA", "Tianma" },
    { "TOL", "TCL" },
    { "TOS", "Toshiba" },
    { "TPV", "TPV" },
    { "TSB", "Toshiba" },
    { "TSD", "TechniSat" },
    { "TTP", "Toshiba" },
    { "UNM", "Unisys" },
    { "VCJ", "JVC" },
    { "VES", "Vestel" },
    { "VIB", "Tatung" },
    { "VIZ", "Vizio" },
    { "VLM", "Lenovo" },
    { "VLV", "Valve" },
    { "VSC", "ViewSonic" },
    { "WAC", "Wacom" },
    { "WDE", "Westinghouse" },
    { "XMI", "Xiaomi" },
    { "YMH", "Yamaha" },
    { "ZCM", "Zenith" },
    { "ZDS", "Zenith" },
    { "ZGT", "Zenith" },
    { "ZSE", "Zenith" }
};

} // namespace

//--------------------------------------------------------------------------------------------------
static int getDataType(const quint8* descriptor)
{
    const quint8 kEdidV1DescriptorFlag[] = { 0x00, 0x00 };

    if (memcmp(descriptor, kEdidV1DescriptorFlag, 2) == 0)
    {
        if (descriptor[2] != 0)
            return kUnknownDescriptor;

        return descriptor[3];
    }

    return kDetailedTimingDescriptor;
}

//--------------------------------------------------------------------------------------------------
static std::string descriptorText(const Edid::MonitorDescriptor* descriptor)
{
    // The string is terminated with 0x0A only when it is shorter than the field; a string of
    // exactly 13 characters occupies the whole field with no terminator.
    const size_t max_length = std::size(descriptor->descriptor_data);

    size_t length = 0;
    while (length < max_length && descriptor->descriptor_data[length] != 0x0A)
        ++length;

    const std::string_view text(reinterpret_cast<const char*>(descriptor->descriptor_data), length);
    return strFromLatin1(strTrimmed(text));
}

//--------------------------------------------------------------------------------------------------
Edid::Edid(const QByteArray& buffer)
    : buffer_(buffer)
{
    static_assert(sizeof(Data) == kMinEdidSize);

    if (buffer_.size() < sizeof(Data))
    {
        LOG(ERROR) << "Invalid EDID data";
        return;
    }

    const Data* edid = reinterpret_cast<const Data*>(buffer_.data());
    if (edid->header != kEdidHeader)
    {
        LOG(ERROR) << "Invalid EDID header:" << edid->header;
        return;
    }

    quint8 checksum = 0;
    for (int index = 0; index < kMinEdidSize; ++index)
        checksum += buffer_[index];

    // The 1-byte sum of all 128 bytes in this EDID block shall equal zero.
    if (checksum)
    {
        LOG(ERROR) << "Invalid EDID checksum:" << checksum;
        return;
    }

    edid_ = edid;
}

//--------------------------------------------------------------------------------------------------
bool Edid::isValid() const
{
    return edid_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
int Edid::weekOfManufacture() const
{
    const quint8 week = edid_->week_of_manufacture;

    // Zero means "not specified" and is very common; 0xFF means that the year field contains
    // the model year (EDID 1.4). Everything out of range is reported as unspecified.
    if (week < kMinWeekOfManufacture || week > kMaxWeekOfManufacture)
        return 0;

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
    const DetailedTimingDescriptor* descriptor =
        reinterpret_cast<const DetailedTimingDescriptor*>(getDescriptor(kDetailedTimingDescriptor));
    if (!descriptor)
        return 0;

    quint32 lo = static_cast<quint32>(descriptor->horizontal_active);
    quint32 hi = ((0xF0 & static_cast<quint32>(descriptor->horizontal_active_blanking)) >> 4);

    return static_cast<int>((hi << 8) | lo);
}

//--------------------------------------------------------------------------------------------------
int Edid::verticalResolution() const
{
    const DetailedTimingDescriptor* descriptor =
        reinterpret_cast<const DetailedTimingDescriptor*>(getDescriptor(kDetailedTimingDescriptor));
    if (!descriptor)
        return 0;

    quint32 lo = static_cast<quint32>(descriptor->vertical_active);
    quint32 hi = ((0xF0 & static_cast<quint32>(descriptor->vertical_active_blanking)) >> 4);

    return static_cast<int>((hi << 8) | lo);
}

//--------------------------------------------------------------------------------------------------
double Edid::gamma() const
{
    const quint8 gamma = edid_->gamma;

    if (gamma == 0xFF)
        return 0.0;

    return (static_cast<double>(gamma) / 100.0) + 1.0;
}

//--------------------------------------------------------------------------------------------------
quint8 Edid::featureSupport() const
{
    return edid_->feature_support;
}

//--------------------------------------------------------------------------------------------------
std::string Edid::getManufacturerSignature() const
{
    BitSet<quint16> id = qbswap(edid_->id_manufacturer_name);

    // Bits 14:10 : first letter (01h = 'A', 02h = 'B', etc.).
    // Bits 9:5 : second letter.
    // Bits 4:0 : third letter.
    const quint16 letters[3] = { id.range(10, 14), id.range(5, 9), id.range(0, 4) };

    char signature[4];

    for (size_t i = 0; i < std::size(letters); ++i)
    {
        // Letter codes run from 01h ('A') to 1Ah ('Z'); anything else is a corrupted field.
        if (letters[i] < 1 || letters[i] > 26)
            return std::string();

        signature[i] = static_cast<char>('A' + letters[i] - 1);
    }

    signature[3] = 0;
    return signature;
}

//--------------------------------------------------------------------------------------------------
std::string Edid::monitorId() const
{
    return getManufacturerSignature() + strHex(edid_->id_product_code, 4, HexCase::LOWER);
}

//--------------------------------------------------------------------------------------------------
std::string Edid::serialNumber() const
{
    const MonitorDescriptor* descriptor =
        reinterpret_cast<const MonitorDescriptor*>(
            getDescriptor(DATA_TYPE_TAG_MONITOR_SERIAL_NUMBER_ASCII));

    if (!descriptor)
        return std::string();

    return descriptorText(descriptor);
}

//--------------------------------------------------------------------------------------------------
const quint8* Edid::getDescriptor(int type) const
{
    size_t count = sizeof(Data::detailed_timing_description) /
        sizeof(Data::detailed_timing_description[0]);

    for (size_t index = 0; index < count; ++index)
    {
        const quint8* descriptor = &edid_->detailed_timing_description[index][0];

        if (getDataType(descriptor) == type)
            return descriptor;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
std::string Edid::manufacturerName() const
{
    const std::string signature = getManufacturerSignature();

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
    const MonitorDescriptor* descriptor =
        reinterpret_cast<const MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MONITOR_NAME_ASCII));

    if (!descriptor)
        return std::string();

    return descriptorText(descriptor);
}

//--------------------------------------------------------------------------------------------------
int Edid::minVerticalRate() const
{
    const MonitorDescriptor* descriptor =
        reinterpret_cast<const MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));
    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[0];
}

//--------------------------------------------------------------------------------------------------
int Edid::maxVerticalRate() const
{
    const MonitorDescriptor* descriptor =
        reinterpret_cast<const MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));
    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[1];
}

//--------------------------------------------------------------------------------------------------
int Edid::minHorizontalRate() const
{
    const MonitorDescriptor* descriptor =
        reinterpret_cast<const MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));
    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[2];
}

//--------------------------------------------------------------------------------------------------
int Edid::maxHorizontalRate() const
{
    const MonitorDescriptor* descriptor =
        reinterpret_cast<const MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));
    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[3];
}

//--------------------------------------------------------------------------------------------------
int Edid::maxSupportedPixelClock() const
{
    const MonitorDescriptor* descriptor =
        reinterpret_cast<const MonitorDescriptor*>(getDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));
    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[4] * 10;
}

//--------------------------------------------------------------------------------------------------
double Edid::pixelClock() const
{
    const DetailedTimingDescriptor* descriptor =
        reinterpret_cast<const DetailedTimingDescriptor*>(getDescriptor(kDetailedTimingDescriptor));
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
quint8 Edid::estabilishedTimings1() const
{
    return edid_->established_timings[0];
}

//--------------------------------------------------------------------------------------------------
quint8 Edid::estabilishedTimings2() const
{
    return edid_->established_timings[1];
}

//--------------------------------------------------------------------------------------------------
quint8 Edid::manufacturersTimings() const
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
    if (index < 0 || index >= standardTimingsCount())
        return false;

    quint8 byte1 = edid_->standard_timing_identification[index][0];
    quint8 byte2 = edid_->standard_timing_identification[index][1];

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
            // 16:10 replaced 1:1 for this code in EDID 1.3 and is kept in later revisions.
            if (edid_->structure_revision >= 3)
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
