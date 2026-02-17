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

#include "driver.h"

/* The driver function table with all function index/address pairs */

/* Начиная с Windows 8 должны быть реализованы ТОЛЬКО перечисленные функции */
static DRVFN DrvFunctionTableWin8[] =
{
    { INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    { INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    { INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    { INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    { INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    { INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
    { INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },
    { INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },
    { INDEX_DrvEscape,                (PFN) DrvEscape             },
    { INDEX_DrvDisableDriver,         (PFN) DrvDisableDriver      },
    { INDEX_DrvGetModes,              (PFN) DrvGetModes           },
    { INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    { INDEX_DrvResetPDEV,             (PFN) DrvResetPDEV          },
    { INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    }
};

static DRVFN DrvFunctionTable[] =
{
    { INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    { INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    { INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    { INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    { INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    { INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
    { INDEX_DrvNotify,                (PFN) DrvNotify             },
    { INDEX_DrvCreateDeviceBitmap,    (PFN) DrvCreateDeviceBitmap },
    { INDEX_DrvDeleteDeviceBitmap,    (PFN) DrvDeleteDeviceBitmap },
    { INDEX_DrvTextOut,               (PFN) DrvTextOut            },
    { INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },
    { INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },
    { INDEX_DrvStrokePath,            (PFN) DrvStrokePath         },
    { INDEX_DrvLineTo,                (PFN) DrvLineTo             },
    { INDEX_DrvFillPath,              (PFN) DrvFillPath           },
    { INDEX_DrvStrokeAndFillPath,     (PFN) DrvStrokeAndFillPath  },
    { INDEX_DrvStretchBlt,            (PFN) DrvStretchBlt         },
    { INDEX_DrvAlphaBlend,            (PFN) DrvAlphaBlend         },
    { INDEX_DrvTransparentBlt,        (PFN) DrvTransparentBlt     },
    { INDEX_DrvGradientFill,          (PFN) DrvGradientFill       },
    { INDEX_DrvPlgBlt,                (PFN) DrvPlgBlt             },
    { INDEX_DrvStretchBltROP,         (PFN) DrvStretchBltROP      },
#if (NTDDI_VERSION >= NTDDI_VISTA)
    { INDEX_DrvRenderHint,            (PFN) DrvRenderHint         },
#endif
    { INDEX_DrvEscape,                (PFN) DrvEscape             },

    //{ INDEX_DrvGetModes,              (PFN) DrvGetModes           },
    { INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    //{ INDEX_DrvResetPDEV,             (PFN) DrvResetPDEV          },
    { INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },
    //{ INDEX_DrvResetDevice,           (PFN) DrvResetDevice        }
};

static DEVMODEW*
MakeDeviceMode(DEVMODEW *DeviceMode, INT Width, INT Height, INT BitsPerPixel)
{
    memset(DeviceMode, 0, sizeof(DEVMODEW));

    wcscpy(DeviceMode->dmDeviceName, DLL_NAME);

    DeviceMode->dmSpecVersion      = DM_SPECVERSION;
    DeviceMode->dmDriverVersion    = DM_SPECVERSION;
    DeviceMode->dmDriverExtra      = 0;
    DeviceMode->dmSize             = sizeof(DEVMODEW);
    DeviceMode->dmBitsPerPel       = BitsPerPixel;
    DeviceMode->dmPelsWidth        = Width;
    DeviceMode->dmPelsHeight       = Height;
    DeviceMode->dmDisplayFrequency = 75;
    DeviceMode->dmDisplayFlags     = 0;
    DeviceMode->dmPanningWidth     = DeviceMode->dmPelsWidth;
    DeviceMode->dmPanningHeight    = DeviceMode->dmPelsHeight;
    DeviceMode->dmFields           = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY;

    return (DEVMODEW *)((LPBYTE)DeviceMode + sizeof(DEVMODEW));
}

static MODESLIST ModesList[] =
{
    /* Width, Height, BitsPerPixel */
    { 800, 600, 8  },
    { 800, 600, 16 },
    { 800, 600, 24 },
    { 800, 600, 32 },

    { 1024, 768, 8  },
    { 1024, 768, 16 },
    { 1024, 768, 24 },
    { 1024, 768, 32 },

    { 1152, 864, 8  },
    { 1152, 864, 16 },
    { 1152, 864, 24 },
    { 1152, 864, 32 },

    { 1280, 720, 8  },
    { 1280, 720, 16 },
    { 1280, 720, 24 },
    { 1280, 720, 32 },

    { 1280, 768, 8  },
    { 1280, 768, 16 },
    { 1280, 768, 24 },
    { 1280, 768, 32 },

    { 1280, 800, 8  },
    { 1280, 800, 16 },
    { 1280, 800, 24 },
    { 1280, 800, 32 },

    { 1280, 960, 8  },
    { 1280, 960, 16 },
    { 1280, 960, 24 },
    { 1280, 960, 32 },

    { 1280, 1024, 8  },
    { 1280, 1024, 16 },
    { 1280, 1024, 24 },
    { 1280, 1024, 32 },

    { 1360, 768, 8  },
    { 1360, 768, 16 },
    { 1360, 768, 24 },
    { 1360, 768, 32 },

    { 1366, 768, 8  },
    { 1366, 768, 16 },
    { 1366, 768, 24 },
    { 1366, 768, 32 },

    { 1600, 900, 8  },
    { 1600, 900, 16 },
    { 1600, 900, 24 },
    { 1600, 900, 32 },

    { 1600, 1024, 8  },
    { 1600, 1024, 16 },
    { 1600, 1024, 24 },
    { 1600, 1024, 32 },

    { 1600, 1200, 8  },
    { 1600, 1200, 16 },
    { 1600, 1200, 24 },
    { 1600, 1200, 32 },

    { 1680, 1050, 8  },
    { 1680, 1050, 16 },
    { 1680, 1050, 24 },
    { 1680, 1050, 32 },

    { 1920, 1080, 8  },
    { 1920, 1080, 16 },
    { 1920, 1080, 24 },
    { 1920, 1080, 32 },

    { 1920, 1200, 8  },
    { 1920, 1200, 16 },
    { 1920, 1200, 24 },
    { 1920, 1200, 32 },

    { 1920, 1440, 8  },
    { 1920, 1440, 16 },
    { 1920, 1440, 24 },
    { 1920, 1440, 32 },

    { 2048, 1536, 8  },
    { 2048, 1536, 16 },
    { 2048, 1536, 24 },
    { 2048, 1536, 32 },

    { 2560, 1440, 8  },
    { 2560, 1440, 16 },
    { 2560, 1440, 24 },
    { 2560, 1440, 32 },

    { 2560, 1600, 8  },
    { 2560, 1600, 16 },
    { 2560, 1600, 24 },
    { 2560, 1600, 32 },

    { 0 }
};

ULONG APIENTRY
DrvGetModes(__in HANDLE hDriver, ULONG cjSize, __out_bcount_opt(cjSize) DEVMODEW *pDeviceMode)
{
    ULONG ulBytesNeeded;
    ULONG ulReturnValue = 0;
 
    UNREFERENCED_PARAMETER(hDriver);

    ulBytesNeeded = sizeof(DEVMODEW) * SIZEOF(ModesList);
 
    if (pDeviceMode == NULL)
    {
        ulReturnValue = ulBytesNeeded;
    }
    else if (cjSize >= ulBytesNeeded)
    {
        DEVMODEW *CurrentMode = pDeviceMode;
        ULONG Index = 0;

        do
        {
            CurrentMode = MakeDeviceMode(CurrentMode,
                                         ModesList[Index].Width,
                                         ModesList[Index].Height,
                                         ModesList[Index].BitsPerPixel);

            Index++;
        }
        while (ModesList[Index].Width != 0);

        ulReturnValue = ulBytesNeeded;
    }
 
    return ulReturnValue;
}

ULONG APIENTRY
DrvSetPointerShape(SURFOBJ *pso,
                   SURFOBJ *psoMask,
                   SURFOBJ *psoColor,
                   XLATEOBJ *pxlo,
                   LONG xHot,
                   LONG yHot,
                   LONG x,
                   LONG y,
                   RECTL *prcl,
                   FLONG fl)
{
    UNREFERENCED_PARAMETER(psoMask);
    UNREFERENCED_PARAMETER(psoColor);
    UNREFERENCED_PARAMETER(pxlo);
    UNREFERENCED_PARAMETER(xHot);
    UNREFERENCED_PARAMETER(yHot);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    UNREFERENCED_PARAMETER(prcl);
    UNREFERENCED_PARAMETER(fl);

    return SPS_ACCEPT_NOEXCLUDE;
}

BOOL APIENTRY
DrvResetPDEV(DHPDEV DhPDEVOld, DHPDEV DhPDEVNew)
{
    UNREFERENCED_PARAMETER(DhPDEVOld);
    UNREFERENCED_PARAMETER(DhPDEVNew);

    return TRUE;
}

VOID APIENTRY
DrvMovePointer(SURFOBJ *pso, LONG x, LONG y, RECTL *prcl)
{
    UNREFERENCED_PARAMETER(pso);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    UNREFERENCED_PARAMETER(prcl);
}

ULONG APIENTRY
DrvResetDevice(IN DHPDEV dhpdev, IN PVOID Reserved)
{
    UNREFERENCED_PARAMETER(dhpdev);
    UNREFERENCED_PARAMETER(Reserved);

    return DRD_SUCCESS;
}

VOID APIENTRY
DrvDisableDriver(VOID)
{

}

/**************************************************************************\
* DrvEnableDriver
*
* Enables the driver by retrieving the drivers function table and version.
*
\**************************************************************************/

BOOL APIENTRY
DrvEnableDriver(ULONG iEngineVersion, ULONG cj, __in_bcount(cj) DRVENABLEDATA *pded)
{
    if (pded == NULL)
    {
        return FALSE;
    }

    /*
     * Engine Version is passed down so future drivers can support previous
     * engine versions.  A next generation driver can support both the old
     * and new engine conventions if told what version of engine it is
     * working with.  For the first version the driver does nothing with it.
     */

    iEngineVersion;

    /* Fill in as much as we can. */

    if (cj >= sizeof(DRVENABLEDATA))
        pded->pdrvfn = DrvFunctionTable;

    if (cj >= (sizeof(ULONG) * 2))
        pded->c = sizeof(DrvFunctionTable) / sizeof(DRVFN);

    /* DDI version this driver was targeted for is passed back to engine. */
    /* Future graphic's engine may break calls down to old driver format. */

    if (cj >= sizeof(ULONG))
    {
        /* DDI_DRIVER_VERSION is now out-dated. See winddi.h */
        /* DDI_DRIVER_VERSION_NT4 is equivalent to the old DDI_DRIVER_VERSION */
        pded->iDriverVersion = DDI_DRIVER_VERSION_NT4;
    }

    return TRUE;
}

static ULONG
GetBitmapType(ULONG BitsPerPixel)
{
    switch (BitsPerPixel)
    {
        case 8:  return BMF_8BPP;
        case 16: return BMF_16BPP;
        case 24: return BMF_24BPP;
    }

    return BMF_32BPP;
}

static ULONG
GetBitsPerPixelFromIFormat(ULONG iFormat)
{
    switch (iFormat)
    {
        case BMF_8BPP: return 8;
        case BMF_16BPP: return 16;
        case BMF_24BPP: return 24;
    }

    return 32;
}

static ULONG
GetHooksType(ULONG BitsPerPixel)
{
    switch (BitsPerPixel)
    {
        case 8:  return HOOKS_BMF8BPP;
        case 16: return HOOKS_BMF16BPP;
        case 24: return HOOKS_BMF24BPP;
    }

    return HOOKS_BMF32BPP;
}

static ULONG
GetBytesPerPixel(ULONG BitmapType)
{
    switch (BitmapType)
    {
        case BMF_8BPP:
            return 1;
        case BMF_16BPP:
            return 2;
        case BMF_24BPP:
            return 3;
    }

    /* 32 Bits per pixel */
    return 4;
}

/**************************************************************************\
* DrvEnablePDEV
*
* DDI function, Enables the Physical Device.
*
* Return Value: device handle to pdev.
*
\**************************************************************************/

DHPDEV APIENTRY
DrvEnablePDEV(__in     DEVMODEW   *pDevmode,                /* Pointer to DEVMODE */
              __in_opt PWSTR       pwszLogAddress,          /* Logical address */
              __in     ULONG       cPatterns,               /* number of patterns */
              __in_opt HSURF      *ahsurfPatterns,          /* return standard patterns */
              __in     ULONG       cjGdiInfo,               /* Length of memory pointed to by pGdiInfo */
              __out_bcount(cjGdiInfo)  ULONG    *pGdiInfo,  /* Pointer to GdiInfo structure */
              __in     ULONG       cjDevInfo,               /* Length of following PDEVINFO structure */
              __out_bcount(cjDevInfo)  DEVINFO  *pDevInfo,  /* physical device information structure */
              __in_opt HDEV        hdev,                    /* HDEV, used for callbacks */
              __in_opt PWSTR       pwszDeviceName,          /* DeviceName - not used */
              __in     HANDLE      hDriver)                 /* Handle to base driver */
{
    GDIINFO GdiInfo;
    DEVINFO DevInfo;
    PDEVICE_DATA DeviceData;
    INT BytesPerPixel;

    UNREFERENCED_PARAMETER(pwszLogAddress);
    UNREFERENCED_PARAMETER(cPatterns);
    UNREFERENCED_PARAMETER(ahsurfPatterns);
    UNREFERENCED_PARAMETER(hdev);
    UNREFERENCED_PARAMETER(pwszDeviceName);

    /* Allocate a physical device structure. */
    DeviceData = (PDEVICE_DATA) EngAllocMem(FL_ZERO_MEMORY, sizeof(DEVICE_DATA), ALLOC_TAG);
    if (DeviceData == NULL)
    {
        return (DHPDEV) 0;
    }

    /* Save the screen handle in the PDEV. */
    DeviceData->hDriver = hDriver;

    /* Get the current screen mode information.  Set up device caps and devinfo. */
    if (bInitPDEV(DeviceData, pDevmode, &GdiInfo, &DevInfo) == FALSE)
    {
        goto Error;
    }
    
    /* Copy the devinfo into the engine buffer. */
    if (sizeof(DEVINFO) > cjDevInfo)
    {
        goto Error;
    }

    RtlCopyMemory(pDevInfo, &DevInfo, sizeof(DEVINFO));

    /* Set the pdevCaps with GdiInfo we have prepared to the list of caps for this pdev. */
    if (sizeof(GDIINFO) > cjGdiInfo)
    {
        goto Error;
    }

    RtlCopyMemory(pGdiInfo, &GdiInfo, sizeof(GDIINFO));

    BytesPerPixel = GetBytesPerPixel(GetBitmapType(DeviceData->ulBitCount));

    DeviceData->pVideoMemory = EngMapFile(L"\\??\\C:\\aspmirv1.dat",
                                          DeviceData->cxScreen * DeviceData->cyScreen * BytesPerPixel,
                                          &DeviceData->hMappedFile);

    return (DHPDEV) DeviceData;

    /* Error case for failure. */
Error:
    SAFE_ENG_FREE_MEM(DeviceData);
    return (DHPDEV) 0;
}

/**************************************************************************\
* DrvCompletePDEV
*
* Store the HPDEV, the engines handle for this PDEV, in the DHPDEV.
*
\**************************************************************************/

VOID APIENTRY
DrvCompletePDEV(DHPDEV DhPDEV, HDEV hDevice)
{
    PDEVICE_DATA DeviceData = (PDEVICE_DATA) DhPDEV;

    if (DeviceData != NULL)
    {
        DeviceData->hdevEng = hDevice;
    }
}

/******************************************* ******************************\
* DrvDisablePDEV
*
* Release the resources allocated in DrvEnablePDEV.  If a surface has been
* enabled DrvDisableSurface will have already been called.
*
\**************************************************************************/

VOID APIENTRY
DrvDisablePDEV(DHPDEV DhPDEV)
{
    PDEVICE_DATA DeviceData = (PDEVICE_DATA) DhPDEV;

    if (DeviceData != NULL)
    {
        EngDeletePalette(DeviceData->hpalDefault);
        SAFE_ENG_FREE_MEM(DhPDEV);
        SAFE_ENG_UNMAP_FILE(DeviceData->hMappedFile);
    }
}

/******************************Public*Routine******************************\
* DrvEnableSurface
*
* Enable the surface for the device.  Hook the calls this driver supports.
*
* Return: Handle to the surface if successful, 0 for failure.
*
\**************************************************************************/

HSURF APIENTRY
DrvEnableSurface(DHPDEV DhPDEV)
{
    PDEVICE_DATA DeviceData;
    HSURF hSurface;
    SIZEL ScreenSize;
    ULONG BitmapType;
    FLONG flHooks;
    ULONG MirrorSize;
    MIRRSURF *MirrorSurface;
    DHSURF DeviceHandleSurface;
    INT BytesPerPixel;

    /* Create engine bitmap around frame buffer. */
    DeviceData = (PDEVICE_DATA) DhPDEV;

    if (DeviceData == NULL)
    {
        return FALSE;
    }

    DeviceData->ptlOrg.x = 0;
    DeviceData->ptlOrg.y = 0;

    ScreenSize.cx = DeviceData->cxScreen;
    ScreenSize.cy = DeviceData->cyScreen;

    BitmapType = GetBitmapType(DeviceData->ulBitCount);
    flHooks = GetHooksType(DeviceData->ulBitCount) | GLOBAL_HOOKS;

    MirrorSize = (ULONG)(sizeof(MIRRSURF) + DeviceData->lDeltaScreen * ScreenSize.cy);

    MirrorSurface = (MIRRSURF *) EngAllocMem(FL_ZERO_MEMORY, MirrorSize, 0x4D495252);
    if (MirrorSurface == NULL)
    {
        return FALSE;
    }

    DeviceHandleSurface = (DHSURF) MirrorSurface;

    hSurface = EngCreateDeviceSurface(DeviceHandleSurface, ScreenSize, BitmapType);
    if (hSurface == (HSURF) 0)
    {
        return FALSE;
    }

    BytesPerPixel = GetBytesPerPixel(BitmapType);

    if (DeviceData->pVideoMemory != NULL && DeviceData->hMappedFile != 0)
    {
        if (EngModifySurface(hSurface,
                             DeviceData->hdevEng,
                             flHooks,
                             0,
                             DeviceHandleSurface,
                             DeviceData->pVideoMemory,
                             DeviceData->cxScreen * BytesPerPixel,
                             NULL) == FALSE)
        {
            SAFE_ENG_DELETE_SURFACE(hSurface);
            return FALSE;
        }
    }

    DeviceData->hsurfEng    = (HSURF) hSurface;
    DeviceData->pvTmpBuffer = (PVOID) DeviceHandleSurface;

    MirrorSurface->cx         = DeviceData->cxScreen;
    MirrorSurface->cy         = DeviceData->cyScreen;
    MirrorSurface->lDelta     = DeviceData->lDeltaScreen;
    MirrorSurface->ulBitCount = DeviceData->ulBitCount;
    MirrorSurface->bIsScreen  = TRUE;

    return hSurface;
}

VOID APIENTRY
DrvNotify(SURFOBJ *pso, ULONG iType, PVOID pvData)
{
    UNREFERENCED_PARAMETER(pso);
    UNREFERENCED_PARAMETER(pvData);
}

VOID APIENTRY
DrvDisableSurface(DHPDEV DhPDEV)
{
    PDEVICE_DATA DeviceData = (PDEVICE_DATA) DhPDEV;

    if (DeviceData != NULL)
    {
        SAFE_ENG_DELETE_SURFACE(DeviceData->hsurfEng);
        /* deallocate MIRRSURF structure. */
        SAFE_ENG_FREE_MEM(DeviceData->pvTmpBuffer);
    }
}

BOOL APIENTRY
DrvCopyBits(OUT SURFOBJ *psoDst,
            IN SURFOBJ *psoSrc,
            IN CLIPOBJ *pco,
            IN XLATEOBJ *pxlo,
            IN RECTL *prclDst,
            IN POINTL *pptlSrc)
{
    return EngCopyBits(psoDst, psoSrc, pco, pxlo, prclDst, pptlSrc);
}

BOOL APIENTRY
DrvBitBlt(IN SURFOBJ *psoDst,
          IN SURFOBJ *psoSrc,
          IN SURFOBJ *psoMask,
          IN CLIPOBJ *pco,
          IN XLATEOBJ *pxlo,
          IN RECTL *prclDst,
          IN POINTL *pptlSrc,
          IN POINTL *pptlMask,
          IN BRUSHOBJ *pbo,
          IN POINTL *pptlBrush,
          IN ROP4 rop4)
{
    return EngBitBlt(psoDst, psoSrc, psoMask, pco, pxlo, prclDst, pptlSrc, pptlMask, pbo, pptlBrush, rop4);
}

BOOL APIENTRY
DrvTextOut(IN SURFOBJ *psoDst,
           IN STROBJ *pstro,
           IN FONTOBJ *pfo,
           IN CLIPOBJ *pco,
           IN RECTL *prclExtra,
           IN RECTL *prclOpaque,
           IN BRUSHOBJ *pboFore,
           IN BRUSHOBJ *pboOpaque,
           IN POINTL *pptlOrg,
           IN MIX mix)
{
    return EngTextOut(psoDst, pstro, pfo, pco, prclExtra, prclOpaque, pboFore, pboOpaque, pptlOrg, mix);
}

BOOL APIENTRY
DrvStrokePath(SURFOBJ*   pso,
              PATHOBJ*   ppo,
              CLIPOBJ*   pco,
              XFORMOBJ*  pxo,
              BRUSHOBJ*  pbo,
              POINTL*    pptlBrush,
              LINEATTRS* pLineAttrs,
              MIX        mix)
{
    return EngStrokePath(pso, ppo, pco, pxo, pbo, pptlBrush, pLineAttrs, mix);
}

BOOL APIENTRY
DrvLineTo(SURFOBJ *pso,
          CLIPOBJ *pco,
          BRUSHOBJ *pbo,
          LONG x1,
          LONG y1,
          LONG x2,
          LONG y2,
          RECTL *prclBounds,
          MIX mix)
{
    return EngLineTo(pso, pco, pbo, x1, y1, x2, y2, prclBounds, mix);
}

BOOL APIENTRY
DrvFillPath(SURFOBJ  *pso,
            PATHOBJ  *ppo,
            CLIPOBJ  *pco,
            BRUSHOBJ *pbo,
            PPOINTL   pptlBrushOrg,
            MIX       mix,
            FLONG     flOptions)
{
    return EngFillPath(pso, ppo,pco, pbo, pptlBrushOrg, mix, flOptions);
}

BOOL APIENTRY
DrvStrokeAndFillPath(SURFOBJ*   pso,
                     PATHOBJ*   ppo,
                     CLIPOBJ*   pco,
                     XFORMOBJ*  pxo,
                     BRUSHOBJ*  pboStroke,
                     LINEATTRS* plineattrs,
                     BRUSHOBJ*  pboFill,
                     POINTL*    pptlBrushOrg,
                     MIX        mixFill,
                     FLONG      flOptions)
{
    return EngStrokeAndFillPath(pso, ppo, pco, pxo, pboStroke, plineattrs, pboFill, pptlBrushOrg, mixFill, flOptions);
}

BOOL APIENTRY
DrvTransparentBlt(SURFOBJ*  psoDst,
                  SURFOBJ*  psoSrc,
                  CLIPOBJ*  pco,
                  XLATEOBJ* pxlo,
                  RECTL*    prclDst,
                  RECTL*    prclSrc,
                  ULONG     iTransColor,
                  ULONG     ulReserved)
{
    return EngTransparentBlt(psoDst, psoSrc, pco, pxlo, prclDst, prclSrc, iTransColor, ulReserved);
}

BOOL APIENTRY
DrvAlphaBlend(SURFOBJ*  psoDst,
              SURFOBJ*  psoSrc,
              CLIPOBJ*  pco,
              XLATEOBJ* pxlo,
              RECTL*    prclDst,
              RECTL*    prclSrc,
              BLENDOBJ* pBlendObj)
{
    return EngAlphaBlend(psoDst, psoSrc, pco, pxlo, prclDst, prclSrc, pBlendObj);
}

BOOL APIENTRY
DrvGradientFill(SURFOBJ*   pso,
                CLIPOBJ*   pco,
                XLATEOBJ*  pxlo,
                TRIVERTEX* pVertex,
                ULONG      nVertex,
                PVOID      pMesh,
                ULONG      nMesh,
                RECTL*     prclExtents,
                POINTL*    pptlDitherOrg,
                ULONG      ulMode)
{
    return EngGradientFill(pso, pco, pxlo, pVertex, nVertex, pMesh, nMesh, prclExtents, pptlDitherOrg, ulMode);
}

BOOL APIENTRY
DrvStretchBlt(SURFOBJ*         psoDst,
              SURFOBJ*         psoSrc,
              SURFOBJ*         psoMsk,
              CLIPOBJ*         pco,
              XLATEOBJ*        pxlo,
              COLORADJUSTMENT* pca,
              POINTL*          pptlHTOrg,
              RECTL*           prclDst,
              RECTL*           prclSrc,
              POINTL*          pptlMsk,
              ULONG            iMode)
{
    return EngStretchBlt(psoDst, psoSrc, psoMsk, pco, pxlo, pca, pptlHTOrg, prclDst, prclSrc, pptlMsk, iMode);
}

BOOL APIENTRY
DrvStretchBltROP(SURFOBJ *psoTrg,
                 SURFOBJ *psoSrc,
                 SURFOBJ *psoMask,
                 CLIPOBJ *pco,
                 XLATEOBJ *pxlo,
                 COLORADJUSTMENT *pca,
                 POINTL *pptlBrushOrg,
                 RECTL *prclTrg,
                 RECTL *prclSrc,
                 POINTL *pptlMask,
                 ULONG iMode,
                 BRUSHOBJ *pbo,
                 ROP4 rop4)
{
    return EngStretchBltROP(psoTrg, psoSrc, psoMask, pco, pxlo, pca, pptlBrushOrg, prclTrg, prclSrc, pptlMask, iMode, pbo, rop4);
}

BOOL APIENTRY
DrvPlgBlt(SURFOBJ *psoTrg,
          SURFOBJ *psoSrc,
          SURFOBJ *psoMsk,
          CLIPOBJ *pco,
          XLATEOBJ *pxlo,
          COLORADJUSTMENT *pca,
          POINTL *pptlBrushOrg,
          POINTFIX *pptfx,
          RECTL *prcl,
          POINTL *pptl,
          ULONG iMode)
{
    return EngPlgBlt(psoTrg, psoSrc, psoMsk, pco, pxlo, pca, pptlBrushOrg, pptfx, prcl, pptl, iMode);
}

HBITMAP APIENTRY
DrvCreateDeviceBitmap(IN DHPDEV DhPDEV, IN SIZEL sizl, IN ULONG iFormat)
{
#if 1
    return NULL;
#else
    MIRRSURF *mirrsurf;
    ULONG mirrorsize;
    DHSURF dhsurf;
    ULONG stride;
    HSURF hsurf;
    PDEVICE_DATA DeviceData;
    ULONG BitsPerPixel;

    DeviceData = (PDEVICE_DATA) DhPDEV;

    if (DeviceData == NULL)
    {
        return NULL;
    }

    if (iFormat == BMF_1BPP || iFormat == BMF_4BPP)
    {
        return NULL;
    }

    BitsPerPixel = GetBitsPerPixelFromIFormat(iFormat);

    /* DWORD align each stride */
    stride = (sizl.cx * (BitsPerPixel / 8) + 3);
    stride -= stride % 4;
   
    mirrorsize = (INT)(sizeof(MIRRSURF) + stride * sizl.cy);

    mirrsurf = (MIRRSURF *) EngAllocMem(FL_ZERO_MEMORY, mirrorsize, 0x4D495252);
    if (mirrsurf == NULL)
    {
        return NULL;
    }

    dhsurf = (DHSURF) mirrsurf;

    hsurf = (HSURF) EngCreateDeviceBitmap(dhsurf, sizl, iFormat);
    if (hsurf == (HSURF) 0)
    {
        return NULL;
    }

    if (EngAssociateSurface(hsurf, DeviceData->hdevEng, GLOBAL_HOOKS) == FALSE)
    {
        EngDeleteSurface(hsurf);
        return NULL;
    }

    mirrsurf->cx         = sizl.cx;
    mirrsurf->cy         = sizl.cy;
    mirrsurf->lDelta     = stride;
    mirrsurf->ulBitCount = BitsPerPixel;
    mirrsurf->bIsScreen  = FALSE;

    return (HBITMAP) hsurf;
#endif
}

VOID APIENTRY
DrvDeleteDeviceBitmap(IN DHSURF DeviceHandleSurface)
{
#if 0
    if (DeviceHandleSurface != NULL)
    {
        MIRRSURF *mirrsurf = (MIRRSURF *) DeviceHandleSurface;

        SAFE_ENG_FREE_MEM(mirrsurf);
    }
#endif
}

#if (NTDDI_VERSION >= NTDDI_VISTA)
LONG APIENTRY
DrvRenderHint(DHPDEV DhPDEV, ULONG NotifyCode, SIZE_T Length, PVOID Data)
{
    UNREFERENCED_PARAMETER(DhPDEV);
    UNREFERENCED_PARAMETER(NotifyCode);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Data);

#if 0
    EngRenderHint(DhPDEV, NotifyCode, Length, Data);
#endif

    return TRUE;
}
#endif /* (NTDDI_VERSION >= NTDDI_VISTA) */

/**************************************************************************\
* DrvAssertMode
*
* Enable/Disable the given device.
*
\**************************************************************************/

BOOL APIENTRY
DrvAssertMode(DHPDEV DhPDEV, BOOL bEnable)
{
    UNREFERENCED_PARAMETER(bEnable);
    UNREFERENCED_PARAMETER(DhPDEV);

    return TRUE;
}

/**************************************************************************\
* DrvEscape
*
* We only handle WNDOBJ_SETUP escape. 
*
\**************************************************************************/

typedef struct _WndObjENUMRECTS
{
    ULONG c;
    RECTL arcl[100];
} WNDOBJENUMRECTS;

static VOID
vDumpWndObjRgn(WNDOBJ *pwo)
{
    ULONG ulRet;

    ulRet = WNDOBJ_cEnumStart(pwo, CT_RECTANGLES, CD_RIGHTDOWN, 100);

    if (ulRet != 0xFFFFFFFF)
    {
        BOOL bMore;
        ULONG i;
        WNDOBJENUMRECTS enumRects;

        do
        {
            bMore = WNDOBJ_bEnum(pwo, sizeof(enumRects), &enumRects.c);

            for (i = 0; i < enumRects.c; i++)
            {
                /*DISPDBG((0,"\nWNDOBJ_rect:[%d,%d][%d,%d]",
                        enumRects.arcl[i].left,
                        enumRects.arcl[i].top,
                        enumRects.arcl[i].right,
                        enumRects.arcl[i].bottom));*/

            }
        }
        while (bMore);
    }
}

static VOID CALLBACK
WndObjCallback(WNDOBJ *pwo, FLONG fl)
{
    if (fl & (WOC_RGN_CLIENT_DELTA |
              WOC_RGN_CLIENT |
              WOC_RGN_SURFACE_DELTA |
              WOC_RGN_SURFACE |
              WOC_CHANGED |
              WOC_DELETE |
              WOC_DRAWN |
              WOC_SPRITE_OVERLAP |
              WOC_SPRITE_NO_OVERLAP
#if (NTDDI_VERSION >= NTDDI_VISTA)
              | WOC_RGN_SPRITE
#endif /* (NTDDI_VERSION >= NTDDI_VISTA) */
              ))
    {
#if (NTDDI_VERSION >= NTDDI_VISTA)
        if (fl & WOC_RGN_SPRITE)
        {
            vDumpWndObjRgn(pwo);
        }
#endif /* (NTDDI_VERSION >= NTDDI_VISTA) */
    }
}

ULONG APIENTRY
DrvEscape(SURFOBJ *pso,
          ULONG iEsc,
          ULONG cjIn,
          PVOID pvIn,
          ULONG cjOut,
          PVOID pvOut)
{
    ULONG ulRet = 0;

    if (pso->dhsurf)
    {

        if (iEsc == WNDOBJ_SETUP)
        {
            WNDOBJ *pwo = NULL;

            pwo = EngCreateWnd(pso,
                               *(HWND*)pvIn,
                               WndObjCallback,
                               WO_DRAW_NOTIFY |
                               WO_RGN_CLIENT |
                               WO_RGN_CLIENT_DELTA |
                               WO_RGN_WINDOW |
                               WO_SPRITE_NOTIFY
#if (NTDDI_VERSION >= NTDDI_VISTA)
                               | WO_RGN_SPRITE
#endif /* (NTDDI_VERSION >= NTDDI_VISTA) */
                               ,
                               0);
            if (pwo != NULL)
            {
                ulRet = 1;
            }
        }
    }

    return ulRet;
}
