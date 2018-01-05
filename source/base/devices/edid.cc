//
// PROJECT:         Aspia
// FILE:            base/devices/edid.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/edid.h"
#include "base/strings/string_printf.h"
#include "base/byte_order.h"
#include "base/bitset.h"
#include "base/logging.h"

namespace aspia {

namespace {

const size_t kMinEdidSize = 0x80; // 128 bytes
const uint64_t kEdidHeader = 0x00FFFFFFFFFFFF00;
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

// static
std::unique_ptr<Edid> Edid::Create(std::unique_ptr<uint8_t[]> data, size_t data_size)
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

            DLOG(WARNING) << "Invalid EDID checksum: " << checksum;
        }
        else
        {
            DLOG(WARNING) << "Invalid EDID header: " << edid->header;
        }
    }
    else
    {
        DLOG(WARNING) << "Invalid EDID data";
    }

    return nullptr;
}

Edid::Edid(std::unique_ptr<uint8_t[]> data, size_t data_size)
    : data_(std::move(data)),
      data_size_(data_size)
{
    edid_ = reinterpret_cast<Data*>(data_.get());
}

int Edid::GetWeekOfManufacture() const
{
    uint8_t week = edid_->week_of_manufacture;

    if (week < kMinWeekOfManufacture || week > kMaxWeekOfManufacture)
    {
        DLOG(WARNING) << "Wrong week field value: " << week;
        return 0;
    }

    return week;
}

int Edid::GetYearOfManufacture() const
{
    // The Year of Manufacture field is used to represent the year of the monitor’s manufacture.
    // The value that is stored is an offset from the year 1990 as derived from the following
    // equation:
    // Value stored = (Year of manufacture - 1990)

    return 1990 + edid_->year_of_manufacture;
}

int Edid::GetEdidVersion() const
{
    return edid_->structure_version;
}

int Edid::GetEdidRevision() const
{
    return edid_->structure_revision;
}

int Edid::GetMaxHorizontalImageSize() const
{
    return edid_->max_horizontal_image_size;
}

int Edid::GetMaxVerticalImageSize() const
{
    return edid_->max_vertical_image_size;
}

int Edid::GetHorizontalResolution() const
{
    DetailedTimingDescriptor* descriptor =
        reinterpret_cast<DetailedTimingDescriptor*>(GetDescriptor(kDetailedTimingDescriptor));

    if (!descriptor)
        return 0;

    uint32_t lo = static_cast<uint32_t>(descriptor->horizontal_active);
    uint32_t hi = ((0xF0 & static_cast<uint32_t>(descriptor->horizontal_active_blanking)) >> 4);

    return (hi << 8) | lo;
}

int Edid::GetVerticalResolution() const
{
    DetailedTimingDescriptor* descriptor =
        reinterpret_cast<DetailedTimingDescriptor*>(GetDescriptor(kDetailedTimingDescriptor));

    if (!descriptor)
        return 0;

    uint32_t lo = static_cast<uint32_t>(descriptor->vertical_active);
    uint32_t hi = ((0xF0 & static_cast<uint32_t>(descriptor->vertical_active_blanking)) >> 4);

    return (hi << 8) | lo;
}

double Edid::GetGamma() const
{
    const uint8_t gamma = edid_->gamma;

    if (gamma == 0xFF)
        return 0.0;

    return (static_cast<double>(gamma) / 100.0) + 1.0;
}

uint8_t Edid::GetFeatureSupport() const
{
    return edid_->feature_support;
}

std::string Edid::GetManufacturerSignature() const
{
    BitSet<uint16_t> id = ByteSwap(edid_->id_manufacturer_name);

    // Bits 14:10 : first letter (01h = 'A', 02h = 'B', etc.).
    // Bits 9:5 : second letter.
    // Bits 4:0 : third letter.
    char signature[4];
    signature[0] = static_cast<char>(id.Range(10, 14)) + 'A' - 1;
    signature[1] = static_cast<char>(id.Range(5, 9)) + 'A' - 1;
    signature[2] = static_cast<char>(id.Range(0, 4)) + 'A' - 1;
    signature[3] = 0;

    return signature;
}

std::string Edid::GetMonitorId() const
{
    return StringPrintf("%s%04X",
                        GetManufacturerSignature().c_str(),
                        edid_->id_product_code);
}

std::string Edid::GetSerialNumber() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(
            GetDescriptor(DATA_TYPE_TAG_MONITOR_SERIAL_NUMBER_ASCII));

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

            sn[i] = descriptor->descriptor_data[i];
        }
    }

    return std::string();
}

uint8_t* Edid::GetDescriptor(int type) const
{
    for (size_t index = 0; index < _countof(Data::detailed_timing_description); ++index)
    {
        uint8_t* descriptor = &edid_->detailed_timing_description[index][0];

        if (GetDataType(descriptor) == type)
            return descriptor;
    }

    return nullptr;
}

std::string Edid::GetManufacturerName() const
{
    std::string signature = GetManufacturerSignature();

    for (size_t i = 0; i < _countof(kManufacturers); ++i)
    {
        if (signature == kManufacturers[i].signature)
            return kManufacturers[i].name;
    }

    return std::string();
}

std::string Edid::GetMonitorName() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(GetDescriptor(DATA_TYPE_TAG_MONITOR_NAME_ASCII));

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

            name[i] = descriptor->descriptor_data[i];
        }
    }

    return std::string();
}

int Edid::GetMinVerticalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(GetDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[0];
}

int Edid::GetMaxVerticalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(GetDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[1];
}

int Edid::GetMinHorizontalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(GetDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[2];
}

int Edid::GetMaxHorizontalRate() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(GetDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[3];
}

int Edid::GetMaxSupportedPixelClock() const
{
    MonitorDescriptor* descriptor =
        reinterpret_cast<MonitorDescriptor*>(GetDescriptor(DATA_TYPE_TAG_MINITOR_RANGE_LIMITS));

    if (!descriptor)
        return 0;

    return descriptor->descriptor_data[4] * 10;
}

double Edid::GetPixelClock() const
{
    DetailedTimingDescriptor* descriptor =
        reinterpret_cast<DetailedTimingDescriptor*>(GetDescriptor(kDetailedTimingDescriptor));

    if (!descriptor)
        return 0;

    return double(descriptor->pixel_clock) / 100.0;
}

Edid::InputSignalType Edid::GetInputSignalType() const
{
    if (edid_->video_input_definition & 0x80)
        return INPUT_SIGNAL_TYPE_DIGITAL;

    return INPUT_SIGNAL_TYPE_ANALOG;
}

uint8_t Edid::GetEstabilishedTimings1() const
{
    return edid_->established_timings[0];
}

uint8_t Edid::GetEstabilishedTimings2() const
{
    return edid_->established_timings[1];
}

uint8_t Edid::GetManufacturersTimings() const
{
    return edid_->manufacturers_reserved_timings;
}

int Edid::GetStandardTimingsCount() const
{
    return _countof(Data::standard_timing_identification);
}

bool Edid::GetStandardTimings(int index, int& width, int& height, int& frequency)
{
    uint8_t byte1 = edid_->standard_timing_identification[index][0];
    uint8_t byte2 = edid_->standard_timing_identification[index][1];

    if (byte1 == 0x01 && byte2 == 0x01)
        return false;

    if (byte1 == 0x00)
        return false;

    int ratio_w;
    int ratio_h;

    width = (byte1 + 31) * 8;

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

    height = width * ratio_h / ratio_w;
    frequency = 60 + (byte2 & 0x3F);

    return true;
}

} // namespace aspia
