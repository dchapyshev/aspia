//
// PROJECT:         Aspia
// FILE:            base/devices/physical_drive_smart.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_SMART_H
#define _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_SMART_H

#include <cstdint>

namespace aspia {

constexpr size_t kMaxSmartAttributesCount = 30;
constexpr size_t kSmartAttributeSize = 12;
constexpr size_t kSmartAttributeDataSize = 512;
constexpr size_t kSmartThresholdSize = 12;
constexpr size_t kSmartThresholdDataSize = 512;

#pragma pack (push, smart_attribute, 1)
typedef struct
{
    uint8_t id;
    uint16_t status_flags;
    uint8_t value;
    uint8_t worst_value;
    uint8_t raw_value[6];
    uint8_t reserved;
} SmartAttribute; // 12 Bytes
#pragma pack (pop, smart_attribute)

static_assert(sizeof(SmartAttribute) == kSmartAttributeSize);

typedef struct
{
    uint16_t revision;
    SmartAttribute attribute[kMaxSmartAttributesCount];
    uint8_t unknown[150];
} SmartAttributeData;

static_assert(sizeof(SmartAttributeData) == kSmartAttributeDataSize);

#pragma pack (push, smart_threshold, 1)
typedef struct
{
    uint8_t attribute_id;
    uint8_t warranty_threshold;
    uint8_t reserved[10];
} SmartThreshold; // 12 Bytes
#pragma pack (pop, smart_threshold)

static_assert(sizeof(SmartThreshold) == kSmartThresholdSize);

typedef struct
{
    uint16_t revision;
    SmartThreshold threshold[kMaxSmartAttributesCount];
    uint8_t unknown[150];
} SmartThresholdData;

static_assert(sizeof(SmartThresholdData) == kSmartThresholdDataSize);

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_SMART_H
