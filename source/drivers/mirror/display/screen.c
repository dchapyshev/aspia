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

#include "driver.h"

#define SYSTM_LOGFONT {16,7,0,0,700,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"System"}
#define HELVE_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"MS Sans Serif"}
#define COURI_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,FIXED_PITCH | FF_DONTCARE, L"Courier"}

/*
 * This is the basic devinfo for a default driver. This is used as a base and customized based
 * on information passed back from the miniport driver.
 */
static const DEVINFO gDevInfoFrameBuffer =
{
    GCAPS_OPAQUERECT | GCAPS_LAYERED,
    //(GCAPS_OPAQUERECT | GCAPS_LAYERED | GCAPS_DITHERONREALIZE | GCAPS_ALTERNATEFILL |
    // GCAPS_WINDINGFILL | GCAPS_MONO_DITHER | GCAPS_GRAY16 | GCAPS_COLOR_DITHER), /* Graphics capabilities */
    SYSTM_LOGFONT,    /* Default font description */
    HELVE_LOGFONT,    /* ANSI variable font description */
    COURI_LOGFONT,    /* ANSI fixed font description */
    0,                /* Count of device fonts */
    0,                /* Preferred DIB format */
    8,                /* Width of color dither */
    8,                /* Height of color dither */
    0,                  /* Default palette to use for this device */
    GCAPS2_SYNCTIMER | GCAPS2_SYNCFLUSH
};

/* This is default palette from Win 3.1 */

#define NUMPALCOLORS    256
#define NUMPALRESERVED  20

static ULONG PalColors[NUMPALCOLORS][4] =
{
    { 0,    0,    0,    0 },  /* 0 */
    { 0x80, 0,    0,    0 },  /* 1 */
    { 0,    0x80, 0,    0 },  /* 2 */
    { 0x80, 0x80, 0,    0 },  /* 3 */
    { 0,    0,    0x80, 0 },  /* 4 */
    { 0x80, 0,    0x80, 0 },  /* 5 */
    { 0,    0x80, 0x80, 0 },  /* 6 */
    { 0xC0, 0xC0, 0xC0, 0 },  /* 7 */

    { 192,  220,  192,  0 },  /* 8 */
    { 166,  202,  240,  0 },  /* 9 */
    { 255,  251,  240,  0 },  /* 10 */
    { 160,  160,  164,  0 },  /* 11 */

    { 0x80, 0x80, 0x80, 0 },  /* 12 */
    { 0xFF, 0,    0,    0 },  /* 13 */
    { 0,    0xFF, 0,    0 },  /* 14 */
    { 0xFF, 0xFF, 0,    0 },  /* 15 */
    { 0,    0,    0xFF, 0 },  /* 16 */
    { 0xFF, 0,    0xFF, 0 },  /* 17 */
    { 0,    0xFF, 0xFF, 0 },  /* 18 */
    { 0xFF, 0xFF, 0xFF, 0 }   /* 19 */
};

/******************************Public*Routine******************************\
* bInitPDEV
*
* Determine the mode we should be in based on the DEVMODE passed in.
* For mirrored devices we don't bother querying the miniport.
*
\**************************************************************************/

BOOL
bInitPDEV(PDEVICE_DATA DeviceData, DEVMODEW *pDevMode, GDIINFO *pGdiInfo, DEVINFO *pDevInfo)
{
    /*
     * Fill in the GDIINFO data structure with the information returned from
     * the kernel driver.
     */

    DeviceData->ulMode       = 0;
    DeviceData->cxScreen     = pDevMode->dmPelsWidth;
    DeviceData->cyScreen     = pDevMode->dmPelsHeight;

    if (pDevMode->dmBitsPerPel != 0)
        DeviceData->ulBitCount = pDevMode->dmBitsPerPel;
    else
        DeviceData->ulBitCount = 32;

    DeviceData->lDeltaScreen = 0;

    if (DeviceData->ulBitCount == 32)
    {
        DeviceData->flRed   = 0x00FF0000;
        DeviceData->flGreen = 0x000FF00;
        DeviceData->flBlue  = 0x00000FF;
    }
    else if (DeviceData->ulBitCount == 16)
    {
        /* 5-6-5 */
        DeviceData->flRed   = 0xF800;
        DeviceData->flGreen = 0x07E0;
        DeviceData->flBlue  = 0x001F;
    }

    pGdiInfo->ulVersion    = GDI_DRIVER_VERSION;
    pGdiInfo->ulTechnology = DT_RASDISPLAY;
    pGdiInfo->ulHorzSize   = 0;
    pGdiInfo->ulVertSize   = 0;

    pGdiInfo->ulHorzRes        = DeviceData->cxScreen;
    pGdiInfo->ulVertRes        = DeviceData->cyScreen;
    pGdiInfo->ulPanningHorzRes = 0;
    pGdiInfo->ulPanningVertRes = 0;
    pGdiInfo->cBitsPixel       = DeviceData->ulBitCount;
    pGdiInfo->cPlanes          = 1;
    pGdiInfo->ulVRefresh       = 1; /* not used */
    pGdiInfo->ulBltAlignment   = 4; /* We don't have accelerated screen-to-screen blts, and any window alignment is okay */

    if (pDevMode->dmLogPixels != 0)
    {
        pGdiInfo->ulLogPixelsX = pDevMode->dmLogPixels;
        pGdiInfo->ulLogPixelsY = pDevMode->dmLogPixels;
    }
    else
    {
        pGdiInfo->ulLogPixelsX = 96;
        pGdiInfo->ulLogPixelsY = 96;
    }

    pGdiInfo->flTextCaps = TC_RA_ABLE;

    pGdiInfo->flRaster = 0; /* flRaster is reserved by DDI */

    if (DeviceData->ulBitCount == 32)
    {
        pGdiInfo->ulDACRed   = 8;
        pGdiInfo->ulDACGreen = 8;
        pGdiInfo->ulDACBlue  = 8;
    }
    else if (DeviceData->ulBitCount == 16)
    {
        /* 5-6-5 */
        pGdiInfo->ulDACRed   = 5;
        pGdiInfo->ulDACGreen = 6;
        pGdiInfo->ulDACBlue  = 5;
    }

    pGdiInfo->ulAspectX    = 0x24; /* One-to-one aspect ratio */
    pGdiInfo->ulAspectY    = 0x24;
    pGdiInfo->ulAspectXY   = 0x33;

    pGdiInfo->xStyleStep   = 1; /* A style unit is 3 pels */
    pGdiInfo->yStyleStep   = 1;
    pGdiInfo->denStyleStep = 3;

    pGdiInfo->ptlPhysOffset.x = 0;
    pGdiInfo->ptlPhysOffset.y = 0;
    pGdiInfo->szlPhysSize.cx  = 0;
    pGdiInfo->szlPhysSize.cy  = 0;

    /* RGB and CMY color info. */

    pGdiInfo->ciDevice.Red.x = 6700;
    pGdiInfo->ciDevice.Red.y = 3300;
    pGdiInfo->ciDevice.Red.Y = 0;

    pGdiInfo->ciDevice.Green.x = 2100;
    pGdiInfo->ciDevice.Green.y = 7100;
    pGdiInfo->ciDevice.Green.Y = 0;

    pGdiInfo->ciDevice.Blue.x = 1400;
    pGdiInfo->ciDevice.Blue.y = 800;
    pGdiInfo->ciDevice.Blue.Y = 0;

    pGdiInfo->ciDevice.AlignmentWhite.x = 3127;
    pGdiInfo->ciDevice.AlignmentWhite.y = 3290;
    pGdiInfo->ciDevice.AlignmentWhite.Y = 0;

    pGdiInfo->ciDevice.RedGamma   = 20000;
    pGdiInfo->ciDevice.GreenGamma = 20000;
    pGdiInfo->ciDevice.BlueGamma  = 20000;

    pGdiInfo->ciDevice.Cyan.x = 0;
    pGdiInfo->ciDevice.Cyan.y = 0;
    pGdiInfo->ciDevice.Cyan.Y = 0;

    pGdiInfo->ciDevice.Magenta.x = 0;
    pGdiInfo->ciDevice.Magenta.y = 0;
    pGdiInfo->ciDevice.Magenta.Y = 0;

    pGdiInfo->ciDevice.Yellow.x = 0;
    pGdiInfo->ciDevice.Yellow.y = 0;
    pGdiInfo->ciDevice.Yellow.Y = 0;

    /* No dye correction for raster displays. */

    pGdiInfo->ciDevice.MagentaInCyanDye   = 0;
    pGdiInfo->ciDevice.YellowInCyanDye    = 0;
    pGdiInfo->ciDevice.CyanInMagentaDye   = 0;
    pGdiInfo->ciDevice.YellowInMagentaDye = 0;
    pGdiInfo->ciDevice.CyanInYellowDye    = 0;
    pGdiInfo->ciDevice.MagentaInYellowDye = 0;

    pGdiInfo->ulDevicePelsDPI = 0; /* For printers only */
    pGdiInfo->ulPrimaryOrder = PRIMARY_ORDER_CBA;

    /*
     * Note: this should be modified later to take into account the size
     * of the display and the resolution.
     */

    pGdiInfo->ulHTPatternSize = HT_PATSIZE_4x4_M;

    pGdiInfo->flHTFlags = HT_FLAG_ADDITIVE_PRIMS;

    /* Fill in the basic devinfo structure */

    *pDevInfo = gDevInfoFrameBuffer;

    /* Fill in the rest of the devinfo and GdiInfo structures. */
#if (NTDDI_VERSION >= NTDDI_VISTA)
    pDevInfo->flGraphicsCaps2 |= GCAPS2_INCLUDEAPIBITMAPS | GCAPS2_EXCLUDELAYERED;
#endif

    pDevInfo->flGraphicsCaps2 |= GCAPS2_ALPHACURSOR | GCAPS2_MOUSETRAILS;

    if (DeviceData->ulBitCount == 8)
    {
        /* It is Palette Managed. */
        pGdiInfo->ulNumColors = 20;
        pGdiInfo->ulNumPalReg = 1 << DeviceData->ulBitCount;

        pDevInfo->flGraphicsCaps |= (GCAPS_PALMANAGED | GCAPS_COLOR_DITHER);

        pGdiInfo->ulHTOutputFormat = HT_FORMAT_8BPP;
        pDevInfo->iDitherFormat = BMF_8BPP;

        /* Assuming palette is orthogonal - all colors are same size. */
        DeviceData->cPaletteShift = 8 - pGdiInfo->ulDACRed;
    }
    else
    {
        pGdiInfo->ulNumColors = (ULONG) (-1);
        pGdiInfo->ulNumPalReg = 0;

        if (DeviceData->ulBitCount == 8)
        {
            pGdiInfo->ulHTOutputFormat = HT_FORMAT_8BPP;
            pDevInfo->iDitherFormat = BMF_8BPP;
        }
        else if (DeviceData->ulBitCount == 16)
        {
            pGdiInfo->ulHTOutputFormat = HT_FORMAT_16BPP;
            pDevInfo->iDitherFormat = BMF_16BPP;
        }
        else if (DeviceData->ulBitCount == 24)
        {
            pGdiInfo->ulHTOutputFormat = HT_FORMAT_24BPP;
            pDevInfo->iDitherFormat = BMF_24BPP;
        }
        else
        {
            pGdiInfo->ulHTOutputFormat = HT_FORMAT_32BPP;
            pDevInfo->iDitherFormat = BMF_32BPP;
        }
    }

    pDevInfo->flGraphicsCaps |= (GCAPS_WINDINGFILL | GCAPS_GEOMETRICWIDE | GCAPS_PANNING);

    /*
     * create remaining palette entries, simple loop to create uniformly
     * distributed color values.
     */

    if (DeviceData->ulBitCount == 8)
    {
        ULONG Red = 0;
        ULONG Green = 0;
        ULONG Blue = 0;
        ULONG i;
    
        for (i = NUMPALRESERVED; i < NUMPALCOLORS; i++)
        {
            PalColors[i][0] = Red;
            PalColors[i][1] = Green;
            PalColors[i][2] = Blue;
            PalColors[i][3] = 0;

            if (!(Red += 32))
                if (!(Green += 32))
                    Blue += 64;
        }

        pDevInfo->hpalDefault = DeviceData->hpalDefault =
            EngCreatePalette(PAL_INDEXED,
                             NUMPALCOLORS, /* cColors */
                             (ULONG *) &PalColors[0], /* pulColors */
                             0,
                             0,
                             0); /* flRed, flGreen, flBlue [not used] */
    }
    else
    {
        pDevInfo->hpalDefault = DeviceData->hpalDefault =
            EngCreatePalette(PAL_BITFIELDS,
                             0,
                             NULL,
                             DeviceData->flRed,
                             DeviceData->flGreen,
                             DeviceData->flBlue);
    }

    return TRUE;
}
