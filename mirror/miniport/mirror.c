/*
 * PROJECT:         Aspia Mirror Driver
 * FILE:            mirror/miniport/mirror.c
 * LICENSE:         See top-level directory
 * PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
 */

#define _WIN32_WINNT 0x0600

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"
#include "ntddvdeo.h"
#include "video.h"

#include "mirror.h"

BOOLEAN
MirrorResetHW(PVOID HwDeviceExtension, ULONG Columns, ULONG Rows)
{
    return TRUE;
}

BOOLEAN
MirrorVidInterrupt(PVOID HwDeviceExtension)
{
    return TRUE;
}

VP_STATUS
MirrorGetPowerState(PVOID HwDeviceExtension,
                    ULONG HwId,
                    PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
    return NO_ERROR;
}

VP_STATUS
MirrorSetPowerState(PVOID HwDeviceExtension,
                    ULONG HwId,
                    PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
    return NO_ERROR;
}

VP_STATUS
MirrorGetChildDescriptor(IN  PVOID HwDeviceExtension,
                         IN  PVIDEO_CHILD_ENUM_INFO ChildEnumInfo,
                         OUT PVIDEO_CHILD_TYPE pChildType,
                         OUT PUCHAR pChildDescriptor,
                         OUT PULONG pUId,
                         OUT PULONG pUnused)
{
    return ERROR_NO_MORE_DEVICES;
}

VP_STATUS __checkReturn
MirrorFindAdapter(__in PVOID HwDeviceExtension,
                  __in PVOID HwContext,
                  __in PWSTR ArgumentString,
                  __inout_bcount(sizeof(VIDEO_PORT_CONFIG_INFO)) PVIDEO_PORT_CONFIG_INFO ConfigInfo,
                  __out PUCHAR Again)
{
    return NO_ERROR;
}

BOOLEAN
MirrorInitialize(PVOID HwDeviceExtension)
{
    return TRUE;
}

BOOLEAN
MirrorStartIO(PVOID HwDeviceExtension, PVIDEO_REQUEST_PACKET RequestPacket)
{
    return TRUE;
}

ULONG
DriverEntry(PVOID Context1, PVOID Context2)
{
    VIDEO_HW_INITIALIZATION_DATA HwInitData;

    /* Zero out structure. */
    VideoPortZeroMemory(&HwInitData, sizeof(VIDEO_HW_INITIALIZATION_DATA));

    /* Specify sizes of structure and extension. */
    HwInitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);

    /* Set entry points. */
    HwInitData.HwFindAdapter             = &MirrorFindAdapter;
    HwInitData.HwInitialize              = &MirrorInitialize;
    HwInitData.HwStartIO                 = &MirrorStartIO;
    HwInitData.HwResetHw                 = &MirrorResetHW;
    HwInitData.HwInterrupt               = &MirrorVidInterrupt;
    HwInitData.HwGetPowerState           = &MirrorGetPowerState;
    HwInitData.HwSetPowerState           = &MirrorSetPowerState;
    HwInitData.HwGetVideoChildDescriptor = &MirrorGetChildDescriptor;

    return VideoPortInitialize(Context1, Context2, &HwInitData, NULL);
}
