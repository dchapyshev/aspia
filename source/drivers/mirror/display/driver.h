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

#define _WIN32_WINNT 0x0600

#include "stddef.h"

#include <windows.h>

#include <stdarg.h>

#include "windef.h"
#include "wingdi.h"
#include "winddi.h"
#include "devioctl.h"
#include "ntddvdeo.h"

#define SIZEOF(a) (sizeof(a) / sizeof(a[0]))

/*
 * always hook these routines to ensure the mirrored driver
 * is called for our surfaces
 */

#define GLOBAL_HOOKS   HOOK_BITBLT | HOOK_TEXTOUT | HOOK_COPYBITS | HOOK_STROKEPATH | HOOK_LINETO | \
                       HOOK_FILLPATH | HOOK_STROKEANDFILLPATH | HOOK_STRETCHBLT | HOOK_ALPHABLEND | \
                       HOOK_TRANSPARENTBLT | HOOK_GRADIENTFILL | HOOK_PLGBLT | HOOK_STRETCHBLTROP

/* Define the functions you want to hook for 8/16/24/32 pel formats */

#define HOOKS_BMF8BPP    0
#define HOOKS_BMF16BPP   0
#define HOOKS_BMF24BPP   0
#define HOOKS_BMF32BPP   0

typedef struct  _PDEV
{
    HANDLE  hDriver;                    /* Handle to \Device\Screen */
    HDEV    hdevEng;                    /* Engine's handle to PDEV */
    HSURF   hsurfEng;                   /* Engine's handle to surface */
    HPALETTE hpalDefault;               /* Handle to the default palette for device. */
    PBYTE   pjScreen;                   /* This is pointer to base screen address */
    ULONG   cxScreen;                   /* Visible screen width */
    ULONG   cyScreen;                   /* Visible screen height */
    POINTL  ptlOrg;                     /* Where this display is anchored in the virtual desktop. */
    ULONG   ulMode;                     /* Mode the mini-port driver is in. */
    LONG    lDeltaScreen;               /* Distance from one scan to the next. */
    ULONG   cScreenSize;                /* size of video memory, including offscreen memory. */
    PVOID   pOffscreenList;             /* linked list of DCI offscreen surfaces. */
    FLONG   flRed;                      /* For bitfields device, Red Mask */
    FLONG   flGreen;                    /* For bitfields device, Green Mask */
    FLONG   flBlue;                     /* For bitfields device, Blue Mask */
    ULONG   cPaletteShift;              /* number of bits the 8-8-8 palette must be shifted by to fit in the hardware palette. */
    ULONG   ulBitCount;                 /* # of bits per pel 8,16,24,32 are only supported. */
    POINTL  ptlHotSpot;                 /* adjustment for pointer hot spot */
    VIDEO_POINTER_CAPABILITIES PointerCapabilities; /* HW pointer abilities */
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes; /* hardware pointer attributes */
    DWORD   cjPointerAttributes;        /* Size of buffer allocated */
    BOOL    fHwCursorActive;            /* Are we currently using the hw cursor */
    PALETTEENTRY *pPal;                 /* If this is pal managed, this is the pal */
    BOOL    bSupportDCI;                /* Does the miniport support DCI? */

    PVOID   pvTmpBuffer;                /* pointer to MIRRSURF bits for screen surface */

    __declspec(align(16)) PVOID  pVideoMemory; /* pointer to shared memory between display driver and application */
    ULONG_PTR hMappedFile;
} DEVICE_DATA, *PDEVICE_DATA;

typedef struct
{
    INT Width;
    INT Height;
    INT BitsPerPixel;
} MODESLIST;

#define DLL_NAME L"aspmirv1"

typedef struct _MIRRSURF
{
    PDEVICE_DATA *pdev;
    ULONG   cx;
    ULONG   cy;
    ULONG   lDelta;
    ULONG   ulBitCount;
    BOOL    bIsScreen;
} MIRRSURF, *PMIRRSURF;

BOOL bInitPDEV(PDEVICE_DATA, PDEVMODEW, GDIINFO *, DEVINFO *);

#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

//
// Determines the size of the DriverExtra information in the DEVMODE
// structure passed to and from the display driver.
//

#define DRIVER_EXTRA_SIZE 0

#define ALLOC_TAG               'bfDD'        // Four byte tag (characters in
                                              // reverse order) used for memory
                                              // allocations

#define SAFE_ENG_FREE_MEM(_a) if (_a != NULL) { EngFreeMem(_a); _a = NULL; }
#define SAFE_ENG_UNMAP_FILE(_a) if (_a != 0) { EngUnmapFile(_a); _a = 0; }
#define SAFE_ENG_DELETE_SURFACE(_a) do { EngDeleteSurface(_a); _a = NULL; } while (0)
