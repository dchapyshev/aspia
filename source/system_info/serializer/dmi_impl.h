//
// PROJECT:         Aspia
// FILE:            system_info/serializer/dmi_impl.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__HARDWARE__DMI_H
#define _ASPIA_BASE__HARDWARE__DMI_H

#include <memory>

namespace aspia {

class DmiTable;

class DmiTableEnumerator
{
public:
    DmiTableEnumerator();

    static const size_t kMaxDataSize = 0xFA00; // 64K

    struct SmBiosData
    {
        quint8 used_20_calling_method;
        quint8 smbios_major_version;
        quint8 smbios_minor_version;
        quint8 dmi_revision;
        quint32 length;
        quint8 smbios_table_data[kMaxDataSize];
    };

    bool isAtEnd() const { return !current_; }
    void advance();
    const DmiTable* table() const;

    quint8 majorVersion() const { return data_.smbios_major_version; }
    quint8 minorVersion() const { return data_.smbios_minor_version; }

private:
    SmBiosData data_;

    const quint8* current_ = nullptr;
    const quint8* next_ = nullptr;

    mutable std::unique_ptr<DmiTable> current_table_;

    Q_DISABLE_COPY(DmiTableEnumerator)
};

class DmiTable
{
public:
    enum Type : quint8
    {
        TYPE_BIOS             = 0x00,
        TYPE_SYSTEM           = 0x01,
        TYPE_BASEBOARD        = 0x02,
        TYPE_CHASSIS          = 0x03,
        TYPE_PROCESSOR        = 0x04,
        TYPE_CACHE            = 0x07,
        TYPE_PORT_CONNECTOR   = 0x08,
        TYPE_SYSTEM_SLOT      = 0x09,
        TYPE_ONBOARD_DEVICE   = 0x0A,
        TYPE_MEMORY_DEVICE    = 0x11,
        TYPE_POINTING_DEVICE  = 0x15,
        TYPE_PORTABLE_BATTERY = 0x16
    };

    Type type() const { return static_cast<Type>(table_[0]); }
    quint8 length() const { return table_[1]; }

protected:
    explicit DmiTable(const quint8* table);

    template<typename T>
    T number(quint8 offset) const
    {
        Q_ASSERT(offset >= length());
        return *reinterpret_cast<const T*>(table_[offset]);
    }

    QString string(quint8 offset) const;

private:
    const quint8* table_;
};

class DmiBiosTable : public DmiTable
{
public:
    QString manufacturer() const;
    QString version() const;
    QString date() const;
    quint64 biosSize() const;
    QString biosRevision() const;
    QString firmwareRevision() const;
    QString address() const;
    quint64 runtimeSize() const;

    struct Characteristics
    {
        bool isa;
        bool mca;
        bool eisa;
        bool pci;
        bool pc_card;
        bool pnp;
        bool apm;
        bool vlb;
        bool escd;
        bool boot_from_cd;
        bool selectable_boot;
        bool socketed_boot_rom;
        bool boot_from_pc_card;
        bool edd;
        bool japanese_floppy_for_nec9800;
        bool japanese_floppy_for_toshiba;
        bool floppy_525_360kb;
        bool floppy_525_12mb;
        bool floppy_35_720kb;
        bool floppy_35_288mb;
        bool print_screen;
        bool keyboard_8042;
        bool serial;
        bool printer;
        bool cga_video;
        bool nec_pc98;
        bool acpi;
        bool usb_legacy;
        bool agp;
        bool i2o_boot;
        bool ls120_boot;
        bool atapi_zip_drive_boot;
        bool ieee1394_boot;
        bool smart_battery;
        bool bios_boot_specification;
        bool key_init_network_boot;
        bool targeted_content_distrib;
        bool uefi;
        bool virtual_machine;
        bool bios_upgradeable;
        bool bios_shadowing;
    };

    void characteristics(Characteristics* result) const;

private:
    friend class DmiTableEnumerator;
    DmiBiosTable(const quint8* table);
};

} // namespace aspia

#endif // _ASPIA_BASE__HARDWARE__DMI_H
