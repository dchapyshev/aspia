//
// PROJECT:         Aspia
// FILE:            category/category_monitor.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/monitor_enumerator.h"
#include "base/strings/string_printf.h"
#include "category/category_monitor.h"
#include "category/category_monitor.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* InputSignalTypeToString(proto::Monitors::InputSignalType value)
{
    switch (value)
    {
        case proto::Monitors::INPUT_SIGNAL_TYPE_DIGITAL:
            return "Digital";

        case proto::Monitors::INPUT_SIGNAL_TYPE_ANALOG:
            return "Analog";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryMonitor::CategoryMonitor()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryMonitor::Name() const
{
    return "Monitor";
}

Category::IconId CategoryMonitor::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryMonitor::Guid() const
{
    return "{281100E4-88ED-4AE2-BC4A-3A37282BBAB5}";
}

void CategoryMonitor::Parse(Table& table, const std::string& data)
{
    proto::Monitors message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Monitors::Item& item = message.item(index);

        Group group = table.AddGroup(item.system_name());

        if (!item.monitor_name().empty())
            group.AddParam("Monitor Name", Value::String(item.monitor_name()));

        if (!item.manufacturer_name().empty())
            group.AddParam("Manufacturer Name", Value::String(item.manufacturer_name()));

        if (!item.monitor_id().empty())
            group.AddParam("Monitor ID", Value::String(item.monitor_id()));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (item.edid_version() != 0)
            group.AddParam("EDID Version", Value::Number(item.edid_version()));

        if (item.edid_revision() != 0)
            group.AddParam("EDID Revision", Value::Number(item.edid_revision()));

        if (item.week_of_manufacture() != 0)
        {
            group.AddParam("Week Of Manufacture", Value::Number(item.week_of_manufacture()));
        }

        if (item.year_of_manufacture() != 0)
        {
            group.AddParam("Year Of Manufacture", Value::Number(item.year_of_manufacture()));
        }

        if (item.gamma() != 0.0)
            group.AddParam("Gamma", Value::String(StringPrintf("%.2f", item.gamma())));

        if (item.max_horizontal_image_size() != 0)
        {
            group.AddParam("Horizontal Image Size",
                           Value::Number(item.max_horizontal_image_size(), "cm"));
        }

        if (item.max_vertical_image_size() != 0)
        {
            group.AddParam("Vertical Image Size",
                           Value::Number(item.max_vertical_image_size(), "cm"));
        }
        if (item.max_horizontal_image_size() != 0 && item.max_vertical_image_size() != 0)
        {
            // Calculate the monitor diagonal by the Pythagorean theorem and translate from
            // centimeters to inches.
            double diagonal_size =
                sqrt((item.max_horizontal_image_size() * item.max_horizontal_image_size()) +
                (item.max_vertical_image_size() * item.max_vertical_image_size())) / 2.54;

            group.AddParam("Diagonal Size", Value::Number(diagonal_size, "\""));
        }

        if (item.horizontal_resolution() != 0)
        {
            group.AddParam("Horizontal Resolution",
                           Value::Number(item.horizontal_resolution(), "px"));
        }

        if (item.vertical_resoulution() != 0)
        {
            group.AddParam("Vertical Resolution",
                           Value::Number(item.vertical_resoulution(), "px"));
        }

        if (item.min_horizontal_rate() != 0)
        {
            group.AddParam("Minimum Horizontal Frequency",
                           Value::Number(item.min_horizontal_rate(), "kHz"));
        }

        if (item.max_horizontal_rate() != 0)
        {
            group.AddParam("Maximum Horizontal Frequency",
                           Value::Number(item.max_horizontal_rate(), "kHz"));
        }

        if (item.min_vertical_rate() != 0)
        {
            group.AddParam("Minimum Vertical Frequency",
                           Value::Number(item.min_vertical_rate(), "Hz"));
        }

        if (item.max_vertical_rate() != 0)
        {
            group.AddParam("Maximum Vertical Frequency",
                           Value::Number(item.max_vertical_rate(), "Hz"));
        }

        if (item.pixel_clock() != 0.0)
        {
            group.AddParam("Pixel Clock",
                           Value::Number(item.pixel_clock(), "MHz"));
        }

        if (item.max_pixel_clock() != 0)
        {
            group.AddParam("Maximum Pixel Clock",
                           Value::Number(item.max_pixel_clock(), "MHz"));
        }

        group.AddParam("Input Signal Type",
                       Value::String(InputSignalTypeToString(item.input_signal_type())));

        {
            Group features_group = group.AddGroup("Supported Features");

            features_group.AddParam("Default GTF", Value::Bool(item.default_gtf_supported()));
            features_group.AddParam("Suspend", Value::Bool(item.suspend_supported()));
            features_group.AddParam("Standby", Value::Bool(item.standby_supported()));
            features_group.AddParam("Active Off", Value::Bool(item.active_off_supported()));
            features_group.AddParam("Preferred Timing Mode", Value::Bool(item.preferred_timing_mode_supported()));
            features_group.AddParam("sRGB", Value::Bool(item.srgb_supported()));
        }

        if (item.timings_size() > 0)
        {
            Group modes_group = group.AddGroup("Supported Video Modes");

            for (int mode = 0; mode < item.timings_size(); ++mode)
            {
                const proto::Monitors::Timing& timing = item.timings(mode);

                modes_group.AddParam(StringPrintf("%dx%d", timing.width(), timing.height()),
                                     Value::Number(timing.frequency(), "Hz"));
            }
        }
    }
}

std::string CategoryMonitor::Serialize()
{
    proto::Monitors message;

    for (MonitorEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        std::unique_ptr<Edid> edid = enumerator.GetEDID();
        if (!edid)
            continue;

        proto::Monitors::Item* item = message.add_item();

        std::string system_name = enumerator.GetFriendlyName();

        if (system_name.empty())
            system_name = enumerator.GetDescription();

        item->set_system_name(system_name);
        item->set_monitor_name(edid->GetMonitorName());
        item->set_manufacturer_name(edid->GetManufacturerName());
        item->set_monitor_id(edid->GetMonitorId());
        item->set_serial_number(edid->GetSerialNumber());
        item->set_edid_version(edid->GetEdidVersion());
        item->set_edid_revision(edid->GetEdidRevision());
        item->set_week_of_manufacture(edid->GetWeekOfManufacture());
        item->set_year_of_manufacture(edid->GetYearOfManufacture());
        item->set_max_horizontal_image_size(edid->GetMaxHorizontalImageSize());
        item->set_max_vertical_image_size(edid->GetMaxVerticalImageSize());
        item->set_horizontal_resolution(edid->GetHorizontalResolution());
        item->set_vertical_resoulution(edid->GetVerticalResolution());
        item->set_gamma(edid->GetGamma());
        item->set_max_horizontal_rate(edid->GetMaxHorizontalRate());
        item->set_min_horizontal_rate(edid->GetMinHorizontalRate());
        item->set_max_vertical_rate(edid->GetMaxVerticalRate());
        item->set_min_vertical_rate(edid->GetMinVerticalRate());
        item->set_pixel_clock(edid->GetPixelClock());
        item->set_max_pixel_clock(edid->GetMaxSupportedPixelClock());

        switch (edid->GetInputSignalType())
        {
            case Edid::INPUT_SIGNAL_TYPE_DIGITAL:
                item->set_input_signal_type(proto::Monitors::INPUT_SIGNAL_TYPE_DIGITAL);
                break;

            case Edid::INPUT_SIGNAL_TYPE_ANALOG:
                item->set_input_signal_type(proto::Monitors::INPUT_SIGNAL_TYPE_ANALOG);
                break;

            default:
                break;
        }

        uint8_t supported_features = edid->GetFeatureSupport();

        if (supported_features & Edid::FEATURE_SUPPORT_DEFAULT_GTF_SUPPORTED)
            item->set_default_gtf_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SUSPEND)
            item->set_suspend_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_STANDBY)
            item->set_standby_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_ACTIVE_OFF)
            item->set_active_off_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_PREFERRED_TIMING_MODE)
            item->set_preferred_timing_mode_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SRGB)
            item->set_srgb_supported(true);

        auto add_timing = [&](int width, int height, int freq)
        {
            proto::Monitors::Timing* timing = item->add_timings();

            timing->set_width(width);
            timing->set_height(height);
            timing->set_frequency(freq);
        };

        uint8_t estabilished_timings1 = edid->GetEstabilishedTimings1();

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_800X600_60HZ)
            add_timing(800, 600, 60);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_800X600_56HZ)
            add_timing(800, 600, 56);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_75HZ)
            add_timing(640, 480, 75);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_72HZ)
            add_timing(640, 480, 72);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_67HZ)
            add_timing(640, 480, 67);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_60HZ)
            add_timing(640, 480, 60);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_720X400_88HZ)
            add_timing(720, 400, 88);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_720X400_70HZ)
            add_timing(720, 400, 70);

        uint8_t estabilished_timings2 = edid->GetEstabilishedTimings2();

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1280X1024_75HZ)
            add_timing(1280, 1024, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_75HZ)
            add_timing(1024, 768, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_70HZ)
            add_timing(1024, 768, 70);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_60HZ)
            add_timing(1024, 768, 60);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_87HZ)
            add_timing(1024, 768, 87);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_832X624_75HZ)
            add_timing(832, 624, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_800X600_75HZ)
            add_timing(800, 600, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_800X600_72HZ)
            add_timing(800, 600, 72);

        uint8_t manufacturer_timings = edid->GetManufacturersTimings();

        if (manufacturer_timings & Edid::MANUFACTURERS_TIMINGS_1152X870_75HZ)
            add_timing(1152, 870, 75);

        for (int index = 0; index < edid->GetStandardTimingsCount(); ++index)
        {
            int width, height, freq;

            if (edid->GetStandardTimings(index, width, height, freq))
                add_timing(width, height, freq);
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
