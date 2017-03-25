/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifndef REGIONSTRUCT_H
#define REGIONSTRUCT_H

#include <stdio.h>
#include <stdint.h>

// Return values from RectIn()
#define rgnOUT    0
#define rgnIN     1
#define rgnPART   2

#define CT_YXBANDED 18

//
// X data types
//

typedef struct _Box
{
    short x1, y1, x2, y2;
} BoxRec, *BoxPtr;

typedef struct _xPoint
{
    int16_t x, y;
} xPoint, *xPointPtr;

typedef xPoint DDXPointRec, *DDXPointPtr;

typedef struct _xRectangle
{
    int16_t x, y;
    uint16_t width, height;
} xRectangle, *xRectanglePtr;

//
// clip region
//

typedef struct _RegData
{
    long size;
    long numRects;
    // BoxRec rects[size]; in memory but not explicitly declared
} RegDataRec, *RegDataPtr;

typedef struct _Region
{
    BoxRec extents;
    RegDataPtr data;
} RegionRec, *RegionPtr;

static const BoxPtr kNullBox = nullptr;
static const RegionPtr kNullRegion = nullptr;

extern BoxRec miEmptyBox;
extern RegDataRec miEmptyData;
extern RegDataRec miBrokenData;

#define REGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
// not a region
#define REGION_NAR(reg)       ((reg)->data == &miBrokenData)
#define REGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define REGION_SIZE(reg)      ((reg)->data ? (reg)->data->size : 0)
#define REGION_RECTS(reg)     ((reg)->data ? (BoxPtr)((reg)->data + 1) : &(reg)->extents)
#define REGION_BOXPTR(reg)    ((BoxPtr)((reg)->data + 1))
#define REGION_BOX(reg,i)     (&REGION_BOXPTR(reg)[i])
#define REGION_TOP(reg)       REGION_BOX(reg, (reg)->data->numRects)
#define REGION_END(reg)       REGION_BOX(reg, (reg)->data->numRects - 1)
#define REGION_SZOF(n)        (sizeof(RegDataRec) + ((n) * sizeof(BoxRec)))

RegionPtr miRegionCreate(BoxPtr rect, int size);
void miRegionInit(RegionPtr pReg, BoxPtr rect, int size);
void miRegionDestroy(RegionPtr pReg);
void miRegionUninit(RegionPtr pReg);
bool miRegionCopy(RegionPtr dst, RegionPtr src);
bool miIntersect(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
bool miUnion(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
bool miRegionAppend(RegionPtr dstrgn, RegionPtr rgn);
bool miRegionValidate(RegionPtr badreg, bool* pOverlap);
RegionPtr miRectsToRegion(int nrects, xRectanglePtr prect, int ctype);
bool miSubtract(RegionPtr regD, RegionPtr regM, RegionPtr regS);
bool miInverse(RegionPtr newReg, RegionPtr reg1, BoxPtr invRect);
int miRectIn(RegionPtr region, BoxPtr prect);
void miTranslateRegion(RegionPtr pReg, int x, int y);
void miRegionReset(RegionPtr pReg, BoxPtr pBox);
bool miRegionBreak(RegionPtr pReg);
bool miPointInRegion(RegionPtr pReg, int x, int y, BoxPtr box);
bool miRegionNotEmpty(RegionPtr pReg);
void miRegionEmpty(RegionPtr pReg);
BoxPtr miRegionExtents(RegionPtr pReg);
bool miRegionsEqual(RegionPtr reg1, RegionPtr reg2);
bool miValidRegion(RegionPtr reg);

#ifdef DEBUG
extern int miPrintRegion(RegionPtr rgn);
#endif

#endif // REGIONSTRUCT_H
