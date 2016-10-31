/*
 * PROJECT:         Aspia Mirror Driver
 * FILE:            mirror/miniport/mirror.h
 * LICENSE:         See top-level directory
 * PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
 */

#ifndef _MIRROR_H
#define _MIRROR_H

VOID DummyInterface();

VP_STATUS __checkReturn
MirrorFindAdapter(__in PVOID HwDeviceExtension,
                  __in PVOID HwContext,
                  __in PWSTR ArgumentString,
                  __inout_bcount(sizeof(VIDEO_PORT_CONFIG_INFO)) PVIDEO_PORT_CONFIG_INFO ConfigInfo,
                  __out PUCHAR Again);

BOOLEAN __checkReturn
MirrorInitialize(__in PVOID HwDeviceExtension);

BOOLEAN __checkReturn
MirrorStartIO(__in PVOID HwDeviceExtension,
              __in_bcount(sizeof(VIDEO_REQUEST_PACKET)) PVIDEO_REQUEST_PACKET RequestPacket);

ULONG __checkReturn
DriverEntry(__in PVOID Context1, __in PVOID Context2);

#endif /* _MIRROR_H */
