//
// SmartCafe Project
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
