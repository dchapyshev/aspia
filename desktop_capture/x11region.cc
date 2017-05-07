/***********************************************************

Copyright 1987, 1988, 1989, 1998  The Open Group

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
 

Copyright 1987, 1988, 1989 by 
Digital Equipment Corporation, Maynard, Massachusetts. 

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

/* The panoramix components contained the following notice */
/****************************************************************
*                                                               *
*    Copyright (c) Digital Equipment Corporation, 1991, 1997    *
*                                                               *
*   All Rights Reserved.  Unpublished rights  reserved  under   *
*   the copyright laws of the United States.                    *
*                                                               *
*   The software contained on this media  is  proprietary  to   *
*   and  embodies  the  confidential  technology  of  Digital   *
*   Equipment Corporation.  Possession, use,  duplication  or   *
*   dissemination of the software and media is authorized only  *
*   pursuant to a valid written license from Digital Equipment  *
*   Corporation.                                                *
*                                                               *
*   RESTRICTED RIGHTS LEGEND   Use, duplication, or disclosure  *
*   by the U.S. Government is subject to restrictions  as  set  *
*   forth in Subparagraph (c)(1)(ii)  of  DFARS  252.227-7013,  *
*   or  in  FAR 52.227-19, as applicable.                       *
*                                                               *
*****************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "x11region.h"

#ifdef WIN32
// warning C4244: '=' : conversion from '__int64' to 'int', possible loss of data
#pragma warning(disable : 4244)
#endif

#if defined (__GNUC__) && !defined (NO_INLINES)
#define INLINE __inline
#else
#define INLINE __forceinline
#endif

#define good(reg) assert(miValidRegion(reg))

/*
 * The functions in this file implement the Region abstraction used extensively
 * throughout the X11 sample server. A Region is simply a set of disjoint
 * (non-overlapping) rectangles, plus an "extent" rectangle which is the
 * smallest single rectangle that contains all the non-overlapping rectangles.
 *
 * A Region is implemented as a "y-x-banded" array of rectangles.  This array
 * imposes two degrees of order.  First, all rectangles are sorted by top side
 * y coordinate first (y1), and then by left side x coordinate (x1).
 *
 * Furthermore, the rectangles are grouped into "bands".  Each rectangle in a
 * band has the same top y coordinate (y1), and each has the same bottom y
 * coordinate (y2).  Thus all rectangles in a band differ only in their left
 * and right side (x1 and x2).  Bands are implicit in the array of rectangles:
 * there is no separate list of band start pointers.
 *
 * The y-x band representation does not minimize rectangles.  In particular,
 * if a rectangle vertically crosses a band (the rectangle has scanlines in 
 * the y1 to y2 area spanned by the band), then the rectangle may be broken
 * down into two or more smaller rectangles stacked one atop the other. 
 *
 *  -----------                    -----------
 *  |         |                    |         |            band 0
 *  |         |  --------            -----------  --------
 *  |         |  |      |  in y-x banded    |         |  |      |   band 1
 *  |         |  |      |  form is        |         |  |      |
 *  -----------  |      |            -----------  --------
 *               |      |                 |      |   band 2
 *               --------                 --------
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible: no two rectangles within a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course).
 *
 * Adam de Boor wrote most of the original region code.  Joel McCormack
 * substantially modified or rewrote most of the core arithmetic routines,
 * and added miRegionValidate in order to support several speed improvements
 * to miValidateTree.  Bob Scheifler changed the representation to be more
 * compact when empty or a single rectangle, and did a bunch of gratuitous
 * reformatting.
 */

/*  true iff two Boxes overlap */
#define EXTENTCHECK(r1,r2)           \
      (!( ((r1)->x2 <= (r2)->x1)  || \
          ((r1)->x1 >= (r2)->x2)  || \
          ((r1)->y2 <= (r2)->y1)  || \
          ((r1)->y1 >= (r2)->y2) ) )

/* true iff (x,y) is in Box */
#define INBOX(r,x,y)                 \
      ( ((r)->x2 >  x) &&            \
        ((r)->x1 <= x) &&            \
        ((r)->y2 >  y) &&            \
        ((r)->y1 <= y) )

/* true iff Box r1 contains Box r2 */
#define SUBSUMES(r1,r2)              \
      ( ((r1)->x1 <= (r2)->x1) &&    \
        ((r1)->x2 >= (r2)->x2) &&    \
        ((r1)->y1 <= (r2)->y1) &&    \
        ((r1)->y2 >= (r2)->y2) )

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define mallocData(n)    (RegDataPtr)malloc(REGION_SZOF(n))
#define freeData(reg)   if ((reg)->data && (reg)->data->size) \
                           free((reg)->data)

#define RECTALLOC_BAIL(pReg,n,bail) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!miRectAlloc(pReg, n)) { goto bail; }

#define RECTALLOC(pReg,n) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!miRectAlloc(pReg, n)) { return false; }

#define ADDRECT(pNextRect,nx1,ny1,nx2,ny2)                               \
{                                                                        \
    pNextRect->x1 = nx1;                                                 \
    pNextRect->y1 = ny1;                                                 \
    pNextRect->x2 = nx2;                                                 \
    pNextRect->y2 = ny2;                                                 \
    ++pNextRect;                                                         \
}

#define NEWRECT(pReg,pNextRect,nx1,ny1,nx2,ny2)                          \
{                                                                        \
    if (!(pReg)->data || ((pReg)->data->numRects == (pReg)->data->size)) \
    {                                                                    \
        if (!miRectAlloc(pReg, 1))                                       \
            return false;                                                \
        pNextRect = REGION_TOP(pReg);                                    \
    }                                                                    \
    ADDRECT(pNextRect,nx1,ny1,nx2,ny2);                                  \
    pReg->data->numRects++;                                              \
    assert(pReg->data->numRects<=pReg->data->size);                      \
}


#define DOWNSIZE(reg,numRects)                                           \
if (((numRects) < ((reg)->data->size >> 1)) && ((reg)->data->size > 50)) \
{                                                                        \
    RegDataPtr NewData;                                                  \
    NewData = (RegDataPtr)realloc((reg)->data, REGION_SZOF(numRects));   \
    if (NewData)                                                         \
    {                                                                    \
        NewData->size = (numRects);                                      \
        (reg)->data = NewData;                                           \
    }                                                                    \
}


BoxRec miEmptyBox = {0, 0, 0, 0};
RegDataRec miEmptyData = {0, 0};

RegDataRec  miBrokenData = {0, 0};
RegionRec   miBrokenRegion = { { 0, 0, 0, 0 }, &miBrokenData };

#ifdef DEBUG
int
miPrintRegion(rgn)
    RegionPtr rgn;
{
    int num, size;
    register int i;
    BoxPtr rects;

    num = REGION_NUM_RECTS(rgn);
    size = REGION_SIZE(rgn);
    rects = REGION_RECTS(rgn);
    ErrorF("num: %d size: %d\n", num, size);
    ErrorF("extents: %d %d %d %d\n",
       rgn->extents.x1, rgn->extents.y1, rgn->extents.x2, rgn->extents.y2);
    for (i = 0; i < num; i++)
      ErrorF("%d %d %d %d \n",
         rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
    ErrorF("\n");
    return(num);
}

#endif /* DEBUG */

bool miRegionsEqual(RegionPtr reg1, RegionPtr reg2)
{
    if (reg1->extents.x1 != reg2->extents.x1)
        return false;

    if (reg1->extents.x2 != reg2->extents.x2)
        return false;

    if (reg1->extents.y1 != reg2->extents.y1)
        return false;

    if (reg1->extents.y2 != reg2->extents.y2)
        return false;

    if (REGION_NUM_RECTS(reg1) != REGION_NUM_RECTS(reg2))
        return false;
    
    BoxPtr rects1 = REGION_RECTS(reg1);
    BoxPtr rects2 = REGION_RECTS(reg2);

    for (int i = 0; i != REGION_NUM_RECTS(reg1); ++i)
    {
        if (rects1[i].x1 != rects2[i].x1)
            return false;

        if (rects1[i].x2 != rects2[i].x2)
            return false;

        if (rects1[i].y1 != rects2[i].y1)
            return false;

        if (rects1[i].y2 != rects2[i].y2)
            return false;
    }

    return true;
}

bool miValidRegion(RegionPtr reg)
{
    if ((reg->extents.x1 > reg->extents.x2) || (reg->extents.y1 > reg->extents.y2))
        return false;

    int numRects = REGION_NUM_RECTS(reg);
    if (!numRects)
    {
        return ((reg->extents.x1 == reg->extents.x2) &&
                (reg->extents.y1 == reg->extents.y2) &&
                (reg->data->size || (reg->data == &miEmptyData)));
    }
    else if (numRects == 1)
    {
        return !reg->data;
    }
    else
    {
        BoxPtr pboxP = REGION_RECTS(reg);
        BoxRec box = *pboxP;
        box.y2 = pboxP[numRects-1].y2;
        BoxPtr pboxN = pboxP + 1;

        for (int i = numRects; --i > 0; pboxP++, pboxN++)
        {
            if ((pboxN->x1 >= pboxN->x2) || (pboxN->y1 >= pboxN->y2))
                return false;

            if (pboxN->x1 < box.x1)
                box.x1 = pboxN->x1;

            if (pboxN->x2 > box.x2)
                box.x2 = pboxN->x2;

            if ((pboxN->y1 < pboxP->y1) ||
                ((pboxN->y1 == pboxP->y1) && ((pboxN->x1 < pboxP->x2) || (pboxN->y2 != pboxP->y2))))
            {
                return false;
            }
        }

        return ((box.x1 == reg->extents.x1) &&
                (box.x2 == reg->extents.x2) &&
                (box.y1 == reg->extents.y1) &&
                (box.y2 == reg->extents.y2));
    }
}

/*****************************************************************
 *   RegionCreate(rect, size)
 *     This routine does a simple malloc to make a structure of
 *     REGION of "size" number of rectangles.
 *****************************************************************/

RegionPtr miRegionCreate(BoxPtr rect, int size)
{
    RegionPtr pReg = (RegionPtr)malloc(sizeof(RegionRec));
    if (!pReg)
        return &miBrokenRegion;

    if (rect)
    {
        pReg->extents = *rect;
        pReg->data = nullptr;
    }
    else
    {
        pReg->extents = miEmptyBox;

        if (size > 1 && (pReg->data = mallocData(size)))
        {
            pReg->data->size = size;
            pReg->data->numRects = 0;
        }
        else
        {
            pReg->data = &miEmptyData;
        }
    }

    return pReg;
}

/*****************************************************************
 *   RegionInit(pReg, rect, size)
 *     Outer region rect is statically allocated.
 *****************************************************************/

void miRegionInit(RegionPtr pReg, BoxPtr rect, int size)
{
    if (rect)
    {
        pReg->extents = *rect;
        pReg->data = nullptr;
    }
    else
    {
        pReg->extents = miEmptyBox;

        if (size > 1)
        {
            pReg->data = mallocData(size);

            if (pReg->data)
            {
                pReg->data->size = size;
                pReg->data->numRects = 0;
            }
            else
            {
                pReg->data = &miEmptyData;
            }
        }
        else
        {
            pReg->data = &miEmptyData;
        }
    }
}

void miRegionDestroy(RegionPtr pReg)
{
    good(pReg);

    freeData(pReg);

    if (pReg != &miBrokenRegion)
        free(pReg);
}

void miRegionUninit(RegionPtr pReg)
{
    good(pReg);
    freeData(pReg);
}

bool miRegionBreak(RegionPtr pReg)
{
    freeData(pReg);
    pReg->extents = miEmptyBox;
    pReg->data = &miBrokenData;
    return false;
}

bool miRectAlloc(RegionPtr pRgn, int n)
{
    if (!pRgn->data)
    {
        ++n;

        pRgn->data = mallocData(n);
        if (!pRgn->data)
            return miRegionBreak(pRgn);

        pRgn->data->numRects = 1;
        *REGION_BOXPTR(pRgn) = pRgn->extents;
    }
    else if (!pRgn->data->size)
    {
        pRgn->data = mallocData(n);
        if (!pRgn->data)
            return miRegionBreak(pRgn);

        pRgn->data->numRects = 0;
    }
    else
    {
        if (n == 1)
        {
            n = pRgn->data->numRects;
            if (n > 500) /* XXX pick numbers out of a hat */
            n = 250;
        }

        n += pRgn->data->numRects;

        RegDataPtr data = (RegDataPtr)realloc(pRgn->data, REGION_SZOF(n));
        if (!data)
            return miRegionBreak(pRgn);

        pRgn->data = data;
    }

    pRgn->data->size = n;

    return true;
}

bool miRegionCopy(RegionPtr dst, RegionPtr src)
{
    good(dst);
    good(src);

    if (dst == src)
        return true;

    dst->extents = src->extents;

    if (!src->data || !src->data->size)
    {
        freeData(dst);
        dst->data = src->data;
        return true;
    }

    if (!dst->data || (dst->data->size < src->data->numRects))
    {
        freeData(dst);
        dst->data = mallocData(src->data->numRects);
        if (!dst->data)
            return miRegionBreak (dst);
        dst->data->size = src->data->numRects;
    }

    dst->data->numRects = src->data->numRects;

    memmove(REGION_BOXPTR(dst),
            REGION_BOXPTR(src),
            dst->data->numRects * sizeof(BoxRec));

    return true;
}


/*======================================================================
 *        Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miCoalesce --
 *    Attempt to merge the boxes in the current band with those in the
 *    previous one.  We are guaranteed that the current band extends to
 *      the end of the rects array.  Used only by miRegionOp.
 *
 * Results:
 *    The new index for the previous band.
 *
 * Side Effects:
 *    If coalescing takes place:
 *        - rectangles in the previous band will have their y2 fields
 *          altered.
 *        - pReg->data->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
INLINE static int miCoalesce(RegionPtr pReg, // Region to coalesce
                             int prevStart,  // Index of start of previous band
                             int curStart)   // Index of start of current band
{
    // Figure out how many rectangles are in the band.
    int numRects = curStart - prevStart; // Number rectangles in both bands
    assert(numRects == pReg->data->numRects - curStart);

    if (!numRects)
        return curStart;

    //
    // The bands may only be coalesced if the bottom of the previous
    // matches the top scanline of the current.
    //
    BoxPtr pPrevBox = REGION_BOX(pReg, prevStart); // Current box in previous band
    BoxPtr pCurBox = REGION_BOX(pReg, curStart);   // Current box in current band

    if (pPrevBox->y2 != pCurBox->y1)
        return curStart;

    //
    // Make sure the bands have boxes in the same places. This assumes that boxes
    // have been added in such a way that they cover the most area possible. I.e.
    // two boxes in a band must have some horizontal space between them.
    //
    int y2 = pCurBox->y2; // Bottom of current band

    do
    {
        if (pPrevBox->x1 != pCurBox->x1 || pPrevBox->x2 != pCurBox->x2)
            return curStart;

        ++pPrevBox;
        ++pCurBox;
        --numRects;
    }
    while (numRects);

    //
    // The bands may be merged, so set the bottom y of each box in the previous band
    // to the bottom y of the current band.
    //
    numRects = curStart - prevStart;
    pReg->data->numRects -= numRects;

    do
    {
        --pPrevBox;
        pPrevBox->y2 = y2;
        --numRects;
    }
    while (numRects);

    return prevStart;
}


// Quicky macro to avoid trivial reject procedure calls to miCoalesce
#define Coalesce(newReg, prevBand, curBand)                       \
    if (curBand - prevBand == newReg->data->numRects - curBand)   \
    {                                                             \
        prevBand = miCoalesce(newReg, prevBand, curBand);         \
    }                                                             \
    else                                                          \
    {                                                             \
        prevBand = curBand;                                       \
    }

/*-
 *-----------------------------------------------------------------------
 * miAppendNonO --
 *    Handle a non-overlapping band for the union and subtract operations.
 *      Just adds the (top/bottom-clipped) rectangles into the region.
 *      Doesn't have to check for subsumption or anything.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    pReg->data->numRects is incremented and the rectangles overwritten
 *    with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */

INLINE static bool miAppendNonO(RegionPtr pReg, BoxPtr r, BoxPtr rEnd, int y1, int y2)
{
    int newRects = rEnd - r;

    assert(y1 < y2);
    assert(newRects != 0);

    // Make sure we have enough space for all rectangles to be added
    RECTALLOC(pReg, newRects);
    BoxPtr pNextRect = REGION_TOP(pReg);
    pReg->data->numRects += newRects;

    do
    {
        assert(r->x1 < r->x2);
        ADDRECT(pNextRect, r->x1, y1, r->x2, y2);
        ++r;
    }
    while (r != rEnd);

    return true;
}

#define FindBand(r, rBandEnd, rEnd, ry1)                              \
{                                                                     \
    ry1 = r->y1;                                                      \
    rBandEnd = r + 1;                                                 \
    while (rBandEnd != rEnd && rBandEnd->y1 == ry1)                   \
    {                                                                 \
        ++rBandEnd;                                                   \
    }                                                                 \
}

#define AppendRegions(newReg, r, rEnd)                                \
{                                                                     \
    int newRects = rEnd - r;                                          \
    if (newRects)                                                     \
    {                                                                 \
        RECTALLOC(newReg, newRects);                                  \
        memmove(REGION_TOP(newReg),  r, newRects * sizeof(BoxRec));   \
        newReg->data->numRects += newRects;                           \
    }                                                                 \
}

/*-
 *-----------------------------------------------------------------------
 * miRegionOp --
 *    Apply an operation to two regions. Called by miUnion, miInverse,
 *    miSubtract, miIntersect....  Both regions MUST have at least one
 *      rectangle, and cannot be the same object.
 *
 * Results:
 *    TRUE if successful.
 *
 * Side Effects:
 *    The new region is overwritten.
 *    pOverlap set to TRUE if overlapFunc ever returns TRUE.
 *
 * Notes:
 *    The idea behind this function is to view the two regions as sets.
 *    Together they cover a rectangle of area that this function divides
 *    into horizontal bands where points are covered only by one region
 *    or by both. For the first case, the nonOverlapFunc is called with
 *    each the band and the band's upper and lower extents. For the
 *    second, the overlapFunc is called to process the entire band. It
 *    is responsible for clipping the rectangles in the band, though
 *    this function provides the boundaries.
 *    At the end of each band, the new region is coalesced, if possible,
 *    to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */

typedef bool (*OverlapProcPtr)(RegionPtr pReg,
                               BoxPtr r1,
                               BoxPtr r1End,
                               BoxPtr r2,
                               BoxPtr r2End,
                               short y1,
                               short y2,
                               bool* pOverlap);

static bool miRegionOp(RegionPtr newReg,           // Place to store result
                       RegionPtr reg1,             // First region in operation
                       RegionPtr reg2,             // 2d region in operation
                       OverlapProcPtr overlapFunc, // Function to call for overlapping bands
                       bool appendNon1,            // Append non-overlapping bands in region 1 ?
                       bool appendNon2,            // Append non-overlapping bands in region 2 ?
                       bool* pOverlap)
{
    BoxPtr r1End;       // End of 1st region
    BoxPtr r2End;       // End of 2d region
    short ybot;         // Bottom of intersection
    short ytop;         // Top of intersection
    RegDataPtr oldData; // Old data for newReg
    int prevBand;       // Index of start of previous band in newReg
    int curBand;        // Index of start of current band in newReg
    BoxPtr r1BandEnd;   // End of current band in r1
    BoxPtr r2BandEnd;   // End of current band in r2
    short top;          // Top of non-overlapping band
    short bot;          // Bottom of non-overlapping band
    int r1y1;           // Temps for r1->y1 and r2->y1
    int r2y1;

    // Break any region computed from a broken region
    if (REGION_NAR (reg1) || REGION_NAR(reg2))
        return miRegionBreak (newReg);
    
    //
    // Initialization:
    //    set r1, r2, r1End and r2End appropriately, save the rectangles
    // of the destination region until the end in case it's one of
    // the two source regions, then mark the "new" region empty, allocating
    // another array of rectangles for it to use.
    //

    BoxPtr r1 = REGION_RECTS(reg1); // Pointer into first region
    int newSize = REGION_NUM_RECTS(reg1);
    r1End = r1 + newSize;

    int numRects = REGION_NUM_RECTS(reg2);
    BoxPtr r2 = REGION_RECTS(reg2); // Pointer into 2d region
    r2End = r2 + numRects;

    assert(r1 != r1End);
    assert(r2 != r2End);

    oldData = nullptr;

    if ((newReg == reg1 && newSize > 1) || (newReg == reg2 && numRects > 1))
    {
        oldData = newReg->data;
        newReg->data = &miEmptyData;
    }

    /* guess at new size */
    if (numRects > newSize)
        newSize = numRects;

    newSize <<= 1;
    if (!newReg->data)
        newReg->data = &miEmptyData;
    else if (newReg->data->size)
        newReg->data->numRects = 0;
    if (newSize > newReg->data->size)
        if (!miRectAlloc(newReg, newSize))
            return false;

    /*
     * Initialize ybot.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     *     In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *    For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */

    ybot = min(r1->y1, r2->y1);
    
    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. miCoalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;
    
    do
    {
        //
        // This algorithm proceeds one source-band (as opposed to a
        // destination band, which is determined by where the two regions
        // intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
        // rectangle after the last one in the current band for their
        // respective regions.
        //
        assert(r1 != r1End);
        assert(r2 != r2End);
    
        FindBand(r1, r1BandEnd, r1End, r1y1);
        FindBand(r2, r2BandEnd, r2End, r2y1);

        //
        // First handle the band that doesn't intersect, if any.
        //
        // Note that attention is restricted to one band in the
        // non-intersecting region at once, so if a region has n
        // bands between the current position and the next place it overlaps
        // the other, this entire loop will be passed through n times.
        //
        if (r1y1 < r2y1)
        {
            if (appendNon1)
            {
                top = max(r1y1, ybot);
                bot = min(r1->y2, r2y1);
                if (top != bot)
                {
                    curBand = newReg->data->numRects;
                    miAppendNonO(newReg, r1, r1BandEnd, top, bot);
                    Coalesce(newReg, prevBand, curBand);
                }
            }
            ytop = r2y1;
        }
        else if (r2y1 < r1y1)
        {
            if (appendNon2)
            {
                top = max(r2y1, ybot);
                bot = min(r2->y2, r1y1);
                if (top != bot)
                {
                    curBand = newReg->data->numRects;
                    miAppendNonO(newReg, r2, r2BandEnd, top, bot);
                    Coalesce(newReg, prevBand, curBand);
                }
            }
            ytop = r1y1;
        }
        else
        {
            ytop = r1y1;
        }

        //
        // Now see if we've hit an intersecting band. The two bands only
        // intersect if ybot > ytop
        //
        ybot = min(r1->y2, r2->y2);
        if (ybot > ytop)
        {
            curBand = newReg->data->numRects;
            (*overlapFunc)(newReg, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot, pOverlap);
            Coalesce(newReg, prevBand, curBand);
        }

        //
        // If we've finished with a band (y2 == ybot) we skip forward
        // in the region to the next band.
        //
        if (r1->y2 == ybot) r1 = r1BandEnd;
        if (r2->y2 == ybot) r2 = r2BandEnd;
    }
    while (r1 != r1End && r2 != r2End);

    /*
     * Deal with whichever region (if any) still has rectangles left.
     *
     * We only need to worry about banding and coalescing for the very first
     * band left.  After that, we can just group all remaining boxes,
     * regardless of how many bands, into one final append to the list.
     */

    if ((r1 != r1End) && appendNon1)
    {
        /* Do first nonOverlap1Func call, which may be able to coalesce */
        FindBand(r1, r1BandEnd, r1End, r1y1);
        curBand = newReg->data->numRects;
        miAppendNonO(newReg, r1, r1BandEnd, max(r1y1, ybot), r1->y2);
        Coalesce(newReg, prevBand, curBand);
        /* Just append the rest of the boxes  */
        AppendRegions(newReg, r1BandEnd, r1End);

    }
    else if ((r2 != r2End) && appendNon2)
    {
        /* Do first nonOverlap2Func call, which may be able to coalesce */
        FindBand(r2, r2BandEnd, r2End, r2y1);
        curBand = newReg->data->numRects;
        miAppendNonO(newReg, r2, r2BandEnd, max(r2y1, ybot), r2->y2);
        Coalesce(newReg, prevBand, curBand);
        /* Append rest of boxes */
        AppendRegions(newReg, r2BandEnd, r2End);
    }

    free(oldData);

    numRects = newReg->data->numRects;
    if (!numRects)
    {
        freeData(newReg);
        newReg->data = &miEmptyData;
    }
    else if (numRects == 1)
    {
        newReg->extents = *REGION_BOXPTR(newReg);
        freeData(newReg);
        newReg->data = nullptr;
    }
    else
    {
        DOWNSIZE(newReg, numRects);
    }

    return true;
}

/*-
 *-----------------------------------------------------------------------
 * miSetExtents --
 *    Reset the extents of a region to what they should be. Called by
 *    miSubtract and miIntersect as they can't figure it out along the
 *    way or do so easily, as miUnion can.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
void miSetExtents(RegionPtr pReg)
{
    if (!pReg->data)
        return;

    if (!pReg->data->size)
    {
        pReg->extents.x2 = pReg->extents.x1;
        pReg->extents.y2 = pReg->extents.y1;
        return;
    }

    BoxPtr pBox = REGION_BOXPTR(pReg);
    BoxPtr pBoxEnd = REGION_END(pReg);

    //
    // Since pBox is the first rectangle in the region, it must have the
    // smallest y1 and since pBoxEnd is the last rectangle in the region,
    // it must have the largest y2, because of banding. Initialize x1 and
    // x2 from  pBox and pBoxEnd, resp., as good things to initialize them
    // to...
    //
    pReg->extents.x1 = pBox->x1;
    pReg->extents.y1 = pBox->y1;
    pReg->extents.x2 = pBoxEnd->x2;
    pReg->extents.y2 = pBoxEnd->y2;

    assert(pReg->extents.y1 < pReg->extents.y2);

    while (pBox <= pBoxEnd)
    {
        if (pBox->x1 < pReg->extents.x1)
            pReg->extents.x1 = pBox->x1;

        if (pBox->x2 > pReg->extents.x2)
            pReg->extents.x2 = pBox->x2;

        ++pBox;
    };

    assert(pReg->extents.x1 < pReg->extents.x2);
}

/*======================================================================
 *        Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * miIntersectO --
 *    Handle an overlapping band for miIntersect.
 *
 * Results:
 *    TRUE if successful.
 *
 * Side Effects:
 *    Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static bool miIntersectO(RegionPtr pReg,
                         BoxPtr r1,
                         BoxPtr r1End,
                         BoxPtr r2,
                         BoxPtr r2End,
                         short y1,
                         short y2,
                         bool* pOverlap)
{
    pOverlap;

    BoxPtr pNextRect = REGION_TOP(pReg);

    assert(y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    do
    {
        int x1 = max(r1->x1, r2->x1);
        int x2 = min(r1->x2, r2->x2);

        //
        // If there's any overlap between the two rectangles, add that
        // overlap to the new region.
        //
        if (x1 < x2)
            NEWRECT(pReg, pNextRect, x1, y1, x2, y2);

        //
        // Advance the pointer(s) with the leftmost right side, since the next
        // rectangle on that list may still overlap the other region's
        // current rectangle.
        //
        if (r1->x2 == x2)
        {
            ++r1;
        }

        if (r2->x2 == x2)
        {
            ++r2;
        }
    }
    while (r1 != r1End && r2 != r2End);

    return true;
}

bool miIntersect(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2)
{
    good(reg1);
    good(reg2);
    good(newReg);

    // check for trivial reject
    if (REGION_NIL(reg1)  || REGION_NIL(reg2) || !EXTENTCHECK(&reg1->extents, &reg2->extents))
    {
        /* Covers about 20% of all cases */
        freeData(newReg);
        newReg->extents.x2 = newReg->extents.x1;
        newReg->extents.y2 = newReg->extents.y1;

        if (REGION_NAR(reg1) || REGION_NAR(reg2))
        {
            newReg->data = &miBrokenData;
            return false;
        }
        else
        {
            newReg->data = &miEmptyData;
        }
    }
    else if (!reg1->data && !reg2->data)
    {
        /* Covers about 80% of cases that aren't trivially rejected */
        newReg->extents.x1 = max(reg1->extents.x1, reg2->extents.x1);
        newReg->extents.y1 = max(reg1->extents.y1, reg2->extents.y1);
        newReg->extents.x2 = min(reg1->extents.x2, reg2->extents.x2);
        newReg->extents.y2 = min(reg1->extents.y2, reg2->extents.y2);

        freeData(newReg);
        newReg->data = nullptr;
    }
    else if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
        return miRegionCopy(newReg, reg1);
    }
    else if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
        return miRegionCopy(newReg, reg2);
    }
    else if (reg1 == reg2)
    {
        return miRegionCopy(newReg, reg1);
    }
    else
    {
        // General purpose intersection
        bool overlap; // result ignored
        if (!miRegionOp(newReg, reg1, reg2, miIntersectO, false, false, &overlap))
            return false;

        miSetExtents(newReg);
    }

    good(newReg);
    return true;
}

#define MERGERECT(r)                                \
{                                                   \
    if (r->x1 <= x2)                                \
    {                                               \
        /* Merge with current rectangle */          \
        if (r->x1 < x2)                             \
            *pOverlap = true;                       \
        if (x2 < r->x2) x2 = r->x2;                 \
    }                                               \
    else                                            \
    {                                               \
        /* Add current rectangle, start new one */  \
        NEWRECT(pReg, pNextRect, x1, y1, x2, y2);   \
        x1 = r->x1;                                 \
        x2 = r->x2;                                 \
    }                                               \
    ++r;                                            \
}

/*======================================================================
 *        Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miUnionO --
 *    Handle an overlapping band for the union operation. Picks the
 *    left-most rectangle each time and merges it into the region.
 *
 * Results:
 *    TRUE if successful.
 *
 * Side Effects:
 *    pReg is overwritten.
 *    pOverlap is set to TRUE if any boxes overlap.
 *
 *-----------------------------------------------------------------------
 */
static bool miUnionO(RegionPtr pReg,
                     BoxPtr r1,
                     BoxPtr r1End,
                     BoxPtr r2,
                     BoxPtr r2End,
                     short y1,
                     short y2,
                     bool* pOverlap)
{
    int x1; // left and right side of current union
    int x2;

    assert (y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    BoxPtr pNextRect = REGION_TOP(pReg);

    // Start off current rectangle
    if (r1->x1 < r2->x1)
    {
        x1 = r1->x1;
        x2 = r1->x2;
        ++r1;
    }
    else
    {
        x1 = r2->x1;
        x2 = r2->x2;
        ++r2;
    }

    while (r1 != r1End && r2 != r2End)
    {
        if (r1->x1 < r2->x1)
            MERGERECT(r1)
        else
            MERGERECT(r2);
    }

    // Finish off whoever (if any) is left
    if (r1 != r1End)
    {
        do
        {
            MERGERECT(r1);
        }
        while (r1 != r1End);
    }
    else if (r2 != r2End)
    {
        do
        {
            MERGERECT(r2);
        }
        while (r2 != r2End);
    }
    
    // Add current rectangle
    NEWRECT(pReg, pNextRect, x1, y1, x2, y2);

    return true;
}

bool miUnion(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2)
{
    // Return TRUE if some overlap between reg1, reg2
    good(reg1);
    good(reg2);
    good(newReg);

    // checks all the simple cases

    // Region 1 and 2 are the same
    if (reg1 == reg2)
    {
        return miRegionCopy(newReg, reg1);
    }

    // Region 1 is empty
    if (REGION_NIL(reg1))
    {
        if (REGION_NAR(reg1))
            return miRegionBreak(newReg);

        if (newReg != reg2)
            return miRegionCopy(newReg, reg2);

        return true;
    }

    // Region 2 is empty
    if (REGION_NIL(reg2))
    {
        if (REGION_NAR(reg2))
            return miRegionBreak(newReg);

        if (newReg != reg1)
            return miRegionCopy(newReg, reg1);

        return true;
    }

    // Region 1 completely subsumes region 2
    if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
        if (newReg != reg1)
            return miRegionCopy(newReg, reg1);

        return true;
    }

    // Region 2 completely subsumes region 1
    if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
        if (newReg != reg2)
            return miRegionCopy(newReg, reg2);

        return true;
    }

    bool overlap; // result ignored

    if (!miRegionOp(newReg, reg1, reg2, miUnionO, true, true, &overlap))
        return false;

    newReg->extents.x1 = min(reg1->extents.x1, reg2->extents.x1);
    newReg->extents.y1 = min(reg1->extents.y1, reg2->extents.y1);
    newReg->extents.x2 = max(reg1->extents.x2, reg2->extents.x2);
    newReg->extents.y2 = max(reg1->extents.y2, reg2->extents.y2);

    good(newReg);
    return true;
}


/*======================================================================
 *        Batch Rectangle Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miRegionAppend --
 * 
 *      "Append" the rgn rectangles onto the end of dstrgn, maintaining
 *      knowledge of YX-banding when it's easy.  Otherwise, dstrgn just
 *      becomes a non-y-x-banded random collection of rectangles, and not
 *      yet a true region.  After a sequence of appends, the caller must
 *      call miRegionValidate to ensure that a valid region is constructed.
 *
 * Results:
 *    TRUE if successful.
 *
 * Side Effects:
 *      dstrgn is modified if rgn has rectangles.
 *
 */
bool miRegionAppend(RegionPtr dstrgn, RegionPtr rgn)
{
    if (REGION_NAR(rgn))
        return miRegionBreak(dstrgn);
    
    if (!rgn->data && (dstrgn->data == &miEmptyData))
    {
        dstrgn->extents = rgn->extents;
        dstrgn->data = nullptr;
        return true;
    }

    int numRects = REGION_NUM_RECTS(rgn);
    if (!numRects)
        return true;

    bool prepend = false;
    int size = numRects;
    int dnumRects = REGION_NUM_RECTS(dstrgn);

    if (!dnumRects && (size < 200))
        size = 200; // XXX pick numbers out of a hat

    RECTALLOC(dstrgn, size);
    BoxPtr old = REGION_RECTS(rgn);

    if (!dnumRects)
    {
        dstrgn->extents = rgn->extents;
    }
    else if (dstrgn->extents.x2 > dstrgn->extents.x1)
    {
        BoxPtr first = old;
        BoxPtr last = REGION_BOXPTR(dstrgn) + (dnumRects - 1);

        if ((first->y1 > last->y2) ||
            ((first->y1 == last->y1) && (first->y2 == last->y2) &&
            (first->x1 > last->x2)))
        {
            if (rgn->extents.x1 < dstrgn->extents.x1)
                dstrgn->extents.x1 = rgn->extents.x1;
            if (rgn->extents.x2 > dstrgn->extents.x2)
                dstrgn->extents.x2 = rgn->extents.x2;
            dstrgn->extents.y2 = rgn->extents.y2;
        }
        else
        {
            first = REGION_BOXPTR(dstrgn);
            last = old + (numRects - 1);
            if ((first->y1 > last->y2) ||
                ((first->y1 == last->y1) && (first->y2 == last->y2) &&
                (first->x1 > last->x2)))
            {
                prepend = true;
                if (rgn->extents.x1 < dstrgn->extents.x1)
                    dstrgn->extents.x1 = rgn->extents.x1;
                if (rgn->extents.x2 > dstrgn->extents.x2)
                    dstrgn->extents.x2 = rgn->extents.x2;
                dstrgn->extents.y1 = rgn->extents.y1;
            }
            else
                dstrgn->extents.x2 = dstrgn->extents.x1;
        }
    }

    BoxPtr new_rgn;

    if (prepend)
    {
        new_rgn = REGION_BOX(dstrgn, numRects);

        if (dnumRects == 1)
        {
            *new_rgn = *REGION_BOXPTR(dstrgn);
        }
        else
        {
            memmove(new_rgn, REGION_BOXPTR(dstrgn), dnumRects * sizeof(BoxRec));
        }

        new_rgn = REGION_BOXPTR(dstrgn);
    }
    else
    {
        new_rgn = REGION_BOXPTR(dstrgn) + dnumRects;
    }

    if (numRects == 1)
    {
        *new_rgn = *old;
    }
    else
    {
        memmove(new_rgn, old, numRects * sizeof(BoxRec));
    }

    dstrgn->data->numRects += numRects;

    return true;
}

#define ExchangeRects(a, b) \
{                           \
    BoxRec t = rects[a];    \
    rects[a] = rects[b];    \
    rects[b] = t;           \
}

static void QuickSortRects(BoxRec rects[], int numRects)
{
    /* Always called with numRects > 1 */

    do
    {
        if (numRects == 2)
        {
            if (rects[0].y1 > rects[1].y1 ||
                (rects[0].y1 == rects[1].y1 && rects[0].x1 > rects[1].x1))
            ExchangeRects(0, 1);
            return;
        }

        /* Choose partition element, stick in location 0 */
        ExchangeRects(0, numRects >> 1);
        int y1 = rects[0].y1;
        int x1 = rects[0].x1;

        /* Partition array */
        int i = 0;
        int j = numRects;
        do
        {
            BoxPtr r = &(rects[i]);
            do
            {
                ++r;
                ++i;
            }
            while (i != numRects &&
                   (r->y1 < y1 || (r->y1 == y1 && r->x1 < x1)));

            r = &(rects[j]);

            do
            {
                --r;
                --j;
            }
            while (y1 < r->y1 || (y1 == r->y1 && x1 < r->x1));

            if (i < j)
                ExchangeRects(i, j);
        }
        while (i < j);

        /* Move partition element back to middle */
        ExchangeRects(0, j);

        /* Recurse */
        if (numRects - j - 1 > 1)
            QuickSortRects(&rects[j+1], numRects - j - 1);

        numRects = j;
    }
    while (numRects > 1);
}

/*-
 *-----------------------------------------------------------------------
 * miRegionValidate --
 * 
 *      Take a ``region'' which is a non-y-x-banded random collection of
 *      rectangles, and compute a nice region which is the union of all the
 *      rectangles.
 *
 * Results:
 *    TRUE if successful.
 *
 * Side Effects:
 *      The passed-in ``region'' may be modified.
 *    pOverlap set to TRUE if any retangles overlapped, else FALSE;
 *
 * Strategy:
 *      Step 1. Sort the rectangles into ascending order with primary key y1
 *        and secondary key x1.
 *
 *      Step 2. Split the rectangles into the minimum number of proper y-x
 *        banded regions.  This may require horizontally merging
 *        rectangles, and vertically coalescing bands.  With any luck,
 *        this step in an identity tranformation (ala the Box widget),
 *        or a coalescing into 1 box (ala Menus).
 *
 *    Step 3. Merge the separate regions down to a single region by calling
 *        miUnion.  Maximize the work each miUnion call does by using
 *        a binary merge.
 *
 *-----------------------------------------------------------------------
 */

bool miRegionValidate(RegionPtr badreg, bool* pOverlap)
{
    // Descriptor for regions under construction  in Step 2.
    typedef struct
    {
        RegionRec reg;
        int prevBand;
        int curBand;
    } RegionInfo;

    int numRects;    // Original numRects for badreg
    RegionInfo* ri;  // Array of current regions
    int numRI;       // Number of entries used in ri
    int sizeRI;      // Number of entries available in ri
    int i;           // Index into rects
    int j;           // Index into ri
    RegionInfo* rit; // &ri[j]
    RegionPtr reg;   // ri[j].reg
    BoxPtr box;      // Current box in rects
    BoxPtr riBox;    // Last box in ri[j].reg
    RegionPtr hreg;  // ri[j_half].reg
    bool ret = true;

    *pOverlap = false;
    if (!badreg->data)
    {
        good(badreg);
        return true;
    }

    numRects = badreg->data->numRects;

    if (!numRects)
    {
        if (REGION_NAR(badreg))
            return false;
        good(badreg);
        return true;
    }

    if (badreg->extents.x1 < badreg->extents.x2)
    {
        if ((numRects) == 1)
        {
            freeData(badreg);
            badreg->data = nullptr;
        }
        else
        {
            DOWNSIZE(badreg, numRects);
        }
        good(badreg);
        return true;
    }

    /* Step 1: Sort the rects array into ascending (y1, x1) order */
    QuickSortRects(REGION_BOXPTR(badreg), numRects);

    /* Step 2: Scatter the sorted array into the minimum number of regions */

    /* Set up the first region to be the first rectangle in badreg */
    /* Note that step 2 code will never overflow the ri[0].reg rects array */
    ri = (RegionInfo*)malloc(4 * sizeof(RegionInfo));
    if (!ri)
        return miRegionBreak(badreg);

    sizeRI = 4;
    numRI = 1;
    ri[0].prevBand = 0;
    ri[0].curBand = 0;
    ri[0].reg = *badreg;
    box = REGION_BOXPTR(&ri[0].reg);
    ri[0].reg.extents = *box;
    ri[0].reg.data->numRects = 1;

    // Now scatter rectangles into the minimum set of valid regions.  If the
    // next rectangle to be added to a region would force an existing rectangle
    // in the region to be split up in order to maintain y-x banding, just
    // forget it.  Try the next region.  If it doesn't fit cleanly into any
    // region, make a new one.

    for (i = numRects; --i > 0;)
    {
        ++box;

        // Look for a region to append box to
        for (j = numRI, rit = ri; --j >= 0; rit++)
        {
            reg = &rit->reg;
            riBox = REGION_END(reg);

            if (box->y1 == riBox->y1 && box->y2 == riBox->y2)
            {
                // box is in same band as riBox.  Merge or append it
                if (box->x1 <= riBox->x2)
                {
                    // Merge it with riBox
                    if (box->x1 < riBox->x2)
                        *pOverlap = true;

                    if (box->x2 > riBox->x2)
                        riBox->x2 = box->x2;
                }
                else
                {
                    RECTALLOC_BAIL(reg, 1, bail);
                    *REGION_TOP(reg) = *box;
                    ++reg->data->numRects;
                }
                goto NextRect;   /* So sue me */
            }
            else if (box->y1 >= riBox->y2)
            {
                /* Put box into new band */
                if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
                if (reg->extents.x1 > box->x1)   reg->extents.x1 = box->x1;
                Coalesce(reg, rit->prevBand, rit->curBand);
                rit->curBand = reg->data->numRects;
                RECTALLOC_BAIL(reg, 1, bail);
                *REGION_TOP(reg) = *box;
                ++reg->data->numRects;
                goto NextRect;
            }
            /* Well, this region was inappropriate.  Try the next one. */
        } /* for j */

        /* Uh-oh.  No regions were appropriate.  Create a new one. */
        if (sizeRI == numRI)
        {
            /* Oops, allocate space for new region information */
            sizeRI <<= 1;
            rit = (RegionInfo*)realloc(ri, sizeRI * sizeof(RegionInfo));
            if (!rit)
                goto bail;

            ri = rit;
            rit = &ri[numRI];
        }

        ++numRI;
        rit->prevBand = 0;
        rit->curBand = 0;
        rit->reg.extents = *box;
        rit->reg.data = nullptr;

        if (!miRectAlloc(&rit->reg, (i+numRI) / numRI)) /* MUST force allocation */
            goto bail;
NextRect: ;
    } /* for i */

    /* Make a final pass over each region in order to Coalesce and set
       extents.x2 and extents.y2 */

    for (j = numRI, rit = ri; --j >= 0; rit++)
    {
        reg = &rit->reg;
        riBox = REGION_END(reg);
        reg->extents.y2 = riBox->y2;

        if (reg->extents.x2 < riBox->x2)
            reg->extents.x2 = riBox->x2;

        Coalesce(reg, rit->prevBand, rit->curBand);
        if (reg->data->numRects == 1) /* keep unions happy below */
        {
            freeData(reg);
            reg->data = nullptr;
        }
    }

    /* Step 3: Union all regions into a single region */
    while (numRI > 1)
    {
        int half = numRI / 2;

        for (j = numRI & 1; j < (half + (numRI & 1)); j++)
        {
            reg = &ri[j].reg;
            hreg = &ri[j + half].reg;
            if (!miRegionOp(reg, reg, hreg, miUnionO, true, true, pOverlap))
                ret = false;
            if (hreg->extents.x1 < reg->extents.x1)
                reg->extents.x1 = hreg->extents.x1;
            if (hreg->extents.y1 < reg->extents.y1)
                reg->extents.y1 = hreg->extents.y1;
            if (hreg->extents.x2 > reg->extents.x2)
                reg->extents.x2 = hreg->extents.x2;
            if (hreg->extents.y2 > reg->extents.y2)
                reg->extents.y2 = hreg->extents.y2;
            freeData(hreg);
        }
        numRI -= half;
    }

    *badreg = ri[0].reg;
    free(ri);
    good(badreg);
    return ret;

bail:
    for (i = 0; i < numRI; i++)
        freeData(&ri[i].reg);

    free(ri);

    return miRegionBreak(badreg);
}

RegionPtr miRectsToRegion(int nrects, xRectanglePtr prect, int ctype)
{
    RegionPtr pRgn = miRegionCreate(kNullBox, 0);
    if (REGION_NAR(pRgn))
        return pRgn;

    if (!nrects)
        return pRgn;

    if (nrects == 1)
    {
        int x1 = prect->x;
        int y1 = prect->y;

        int x2 = x1 + prect->width;
        int y2 = y1 + prect->height;

        if (x2 > SHRT_MAX)
            x2 = SHRT_MAX;

        if (y2 > SHRT_MAX)
            y2 = SHRT_MAX;

        if (x1 != x2 && y1 != y2)
        {
            pRgn->extents.x1 = x1;
            pRgn->extents.y1 = y1;
            pRgn->extents.x2 = x2;
            pRgn->extents.y2 = y2;
            pRgn->data = nullptr;
        }

        return pRgn;
    }

    RegDataPtr pData = mallocData(nrects);
    if (!pData)
    {
        miRegionBreak(pRgn);
        return pRgn;
    }

    BoxPtr pBox = (BoxPtr) (pData + 1);

    for (int i = nrects; --i >= 0; prect++)
    {
        int x1 = prect->x;
        int y1 = prect->y;

        int x2 = x1 + (int) prect->width;
        int y2 = y1 + (int) prect->height;

        if (x2 > SHRT_MAX)
            x2 = SHRT_MAX;

        if (y2 > SHRT_MAX)
            y2 = SHRT_MAX;

        if (x1 != x2 && y1 != y2)
        {
            pBox->x1 = x1;
            pBox->y1 = y1;
            pBox->x2 = x2;
            pBox->y2 = y2;
            ++pBox;
        }
    }

    if (pBox != (BoxPtr) (pData + 1))
    {
        pData->size = nrects;
        pData->numRects = pBox - (BoxPtr) (pData + 1);
        pRgn->data = pData;

        if (ctype != CT_YXBANDED)
        {
            bool overlap; // result ignored
            pRgn->extents.x1 = pRgn->extents.x2 = 0;
            miRegionValidate(pRgn, &overlap);
        }
        else
        {
            miSetExtents(pRgn);
        }

        good(pRgn);
    }
    else
    {
        free(pData);
    }

    return pRgn;
}

/*======================================================================
 *               Region Subtraction
 *====================================================================*/


/*-
 *-----------------------------------------------------------------------
 * miSubtractO --
 *    Overlapping band subtraction. x1 is the left-most point not yet
 *    checked.
 *
 * Results:
 *    TRUE if successful.
 *
 * Side Effects:
 *    pReg may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static bool
miSubtractO(RegionPtr pReg,
            BoxPtr r1,
            BoxPtr r1End,
            BoxPtr r2,
            BoxPtr r2End,
            short y1,
            short y2,
            bool* pOverlap)
{
    pOverlap;

    int x1 = r1->x1;
    
    assert(y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    BoxPtr pNextRect = REGION_TOP(pReg);

    do
    {
        if (r2->x2 <= x1)
        {
            // Subtrahend entirely to left of minuend: go to next subtrahend.
            ++r2;
        }
        else if (r2->x1 <= x1)
        {
            // Subtrahend preceeds minuend: nuke left edge of minuend.
            x1 = r2->x2;
            if (x1 >= r1->x2)
            {
                //
                // Minuend completely covered: advance to next minuend and
                // reset left fence to edge of new minuend.
                //
                ++r1;
                if (r1 != r1End)
                    x1 = r1->x1;
            }
            else
            {
                // Subtrahend now used up since it doesn't extend beyond minuend
                ++r2;
            }
        }
        else if (r2->x1 < r1->x2)
        {
            //
            // Left part of subtrahend covers part of minuend: add uncovered
            // part of minuend to region and skip to next subtrahend.
            //
            assert(x1<r2->x1);
            NEWRECT(pReg, pNextRect, x1, y1, r2->x1, y2);

            x1 = r2->x2;
            if (x1 >= r1->x2)
            {
                // Minuend used up: advance to new...
                ++r1;
                if (r1 != r1End)
                    x1 = r1->x1;
            }
            else
            {
                // Subtrahend used up
                ++r2;
            }
        }
        else
        {
            // Minuend used up: add any remaining piece before advancing.
            if (r1->x2 > x1)
                NEWRECT(pReg, pNextRect, x1, y1, r1->x2, y2);
            ++r1;
            if (r1 != r1End)
                x1 = r1->x1;
        }
    }
    while ((r1 != r1End) && (r2 != r2End));

    // Add remaining minuend rectangles to region.
    while (r1 != r1End)
    {
        assert(x1<r1->x2);
        NEWRECT(pReg, pNextRect, x1, y1, r1->x2, y2);
        ++r1;
        if (r1 != r1End)
            x1 = r1->x1;
    }

    return true;
}
    
/*-
 *-----------------------------------------------------------------------
 * miSubtract --
 *    Subtract regS from regM and leave the result in regD.
 *    S stands for subtrahend, M for minuend and D for difference.
 *
 * Results:
 *    TRUE if successful.
 *
 * Side Effects:
 *    regD is overwritten.
 *
 *-----------------------------------------------------------------------
 */
bool miSubtract(RegionPtr regD, RegionPtr regM, RegionPtr regS)
{
    good(regM);
    good(regS);
    good(regD);

    // check for trivial rejects
    if (REGION_NIL(regM) || REGION_NIL(regS) || !EXTENTCHECK(&regM->extents, &regS->extents))
    {
        if (REGION_NAR(regS))
            return miRegionBreak(regD);

        return miRegionCopy(regD, regM);
    }
    else if (regM == regS)
    {
        freeData(regD);
        regD->extents.x2 = regD->extents.x1;
        regD->extents.y2 = regD->extents.y1;
        regD->data = &miEmptyData;
        return true;
    }

    bool overlap; // result ignored
 
    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky substraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    if (!miRegionOp(regD, regM, regS, miSubtractO, true, false, &overlap))
        return false;

    /*
     * Can't alter RegD's extents before we call miRegionOp because
     * it might be one of the source regions and miRegionOp depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(regD);
    good(regD);

    return true;
}

/*======================================================================
 *        Region Inversion
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miInverse --
 *    Take a region and a box and return a region that is everything
 *    in the box but not in the region. The careful reader will note
 *    that this is the same as subtracting the region from the box...
 *
 * Results:
 *    TRUE.
 *
 * Side Effects:
 *    newReg is overwritten.
 *
 *-----------------------------------------------------------------------
 */
bool miInverse(RegionPtr newReg, // Destination region
               RegionPtr reg1,   // Region to invert
               BoxPtr invRect)   // Bounding box for inversion
{
    RegionRec invReg; // Quick and dirty region made from the bounding box
    bool overlap;     // result ignored

    good(reg1);
    good(newReg);

    // check for trivial rejects
    if (REGION_NIL(reg1) || !EXTENTCHECK(invRect, &reg1->extents))
    {
        if (REGION_NAR(reg1))
            return miRegionBreak(newReg);

        newReg->extents = *invRect;

        freeData(newReg);
        newReg->data = nullptr;

        return true;
    }

    // Add those rectangles in region 1 that aren't in region 2,
    // do yucky substraction for overlaps, and
    // just throw away rectangles in region 2 that aren't in region 1
    invReg.extents = *invRect;
    invReg.data = nullptr;
    if (!miRegionOp(newReg, &invReg, reg1, miSubtractO, true, false, &overlap))
        return false;

    //
    // Can't alter newReg's extents before we call miRegionOp because
    // it might be one of the source regions and miRegionOp depends
    // on the extents of those regions being unaltered. Besides, this
    // way there's no checking against rectangles that will be nuked
    // due to coalescing, so we have to examine fewer rectangles.
    //
    miSetExtents(newReg);
    good(newReg);

    return true;
}

/*
 *   RectIn(region, rect)
 *   This routine takes a pointer to a region and a pointer to a box
 *   and determines if the box is outside/inside/partly inside the region.
 *
 *   The idea is to travel through the list of rectangles trying to cover the
 *   passed box with them. Anytime a piece of the rectangle isn't covered
 *   by a band of rectangles, partOut is set TRUE. Any time a rectangle in
 *   the region covers part of the box, partIn is set TRUE. The process ends
 *   when either the box has been completely covered (we reached a band that
 *   doesn't overlap the box, partIn is TRUE and partOut is false), the
 *   box has been partially covered (partIn == partOut == TRUE -- because of
 *   the banding, the first time this is true we know the box is only
 *   partially in the region) or is outside the region (we reached a band
 *   that doesn't overlap the box at all and partIn is false)
 */

int miRectIn(RegionPtr region, BoxPtr prect)
{
    BoxPtr pbox;
    BoxPtr pboxEnd;

    good(region);
    int numRects = REGION_NUM_RECTS(region);
    /* useful optimization */
    if (!numRects || !EXTENTCHECK(&region->extents, prect))
        return(rgnOUT);

    if (numRects == 1)
    {
        /* We know that it must be rgnIN or rgnPART */
        if (SUBSUMES(&region->extents, prect))
            return(rgnIN);
        else
            return(rgnPART);
    }

    bool partOut = false;
    bool partIn = false;

    /* (x,y) starts at upper left of rect, moving to the right and down */
    int x = prect->x1;
    int y = prect->y1;

    /* can stop when both partOut and partIn are TRUE, or we reach prect->y2 */
    for (pbox = REGION_BOXPTR(region), pboxEnd = pbox + numRects;
         pbox != pboxEnd;
         pbox++)
    {

        if (pbox->y2 <= y)
           continue;    /* getting up to speed or skipping remainder of band */

        if (pbox->y1 > y)
        {
           partOut = true;      /* missed part of rectangle above */
           if (partIn || (pbox->y1 >= prect->y2))
              break;
           y = pbox->y1;        /* x guaranteed to be == prect->x1 */
        }

        if (pbox->x2 <= x)
           continue;            /* not far enough over yet */

        if (pbox->x1 > x)
        {
           partOut = true;      /* missed part of rectangle to left */
           if (partIn)
              break;
        }

        if (pbox->x1 < prect->x2)
        {
            partIn = true;      /* definitely overlap */
            if (partOut)
               break;
        }

        if (pbox->x2 >= prect->x2)
        {
           y = pbox->y2;        /* finished with this band */
           if (y >= prect->y2)
              break;
           x = prect->x1;       /* reset x out to left again */
        }
        else
        {
            /*
             * Because boxes in a band are maximal width, if the first box
             * to overlap the rectangle doesn't completely cover it in that
             * band, the rectangle must be partially out, since some of it
             * will be uncovered in that band. partIn will have been set true
             * by now...
             */
            partOut = true;
            break;
        }
    }

    return (partIn ? ((y < prect->y2) ? rgnPART : rgnIN) : rgnOUT);
}

/* TranslateRegion(pReg, x, y)
   translates in place
*/

void miTranslateRegion(RegionPtr pReg, int x, int y)
{
    int x1, x2, y1, y2;
    int nbox;
    BoxPtr pbox;

    good(pReg);
    pReg->extents.x1 = x1 = pReg->extents.x1 + x;
    pReg->extents.y1 = y1 = pReg->extents.y1 + y;
    pReg->extents.x2 = x2 = pReg->extents.x2 + x;
    pReg->extents.y2 = y2 = pReg->extents.y2 + y;

    if (((x1 - SHRT_MIN)|(y1 - SHRT_MIN)|(SHRT_MAX - x2)|(SHRT_MAX - y2)) >= 0)
    {
        if (pReg->data && (nbox = pReg->data->numRects))
        {
            for (pbox = REGION_BOXPTR(pReg); nbox--; pbox++)
            {
                pbox->x1 += x;
                pbox->y1 += y;
                pbox->x2 += x;
                pbox->y2 += y;
            }
        }
        return;
    }

    if (((x2 - SHRT_MIN)|(y2 - SHRT_MIN)|(SHRT_MAX - x1)|(SHRT_MAX - y1)) <= 0)
    {
        pReg->extents.x2 = pReg->extents.x1;
        pReg->extents.y2 = pReg->extents.y1;
        freeData(pReg);
        pReg->data = &miEmptyData;
        return;
    }

    if (x1 < SHRT_MIN)
        pReg->extents.x1 = SHRT_MIN;
    else if (x2 > SHRT_MAX)
        pReg->extents.x2 = SHRT_MAX;
    if (y1 < SHRT_MIN)
        pReg->extents.y1 = SHRT_MIN;
    else if (y2 > SHRT_MAX)
        pReg->extents.y2 = SHRT_MAX;

    if (pReg->data && (nbox = pReg->data->numRects))
    {
        BoxPtr pboxout;

        for (pboxout = pbox = REGION_BOXPTR(pReg); nbox--; pbox++)
        {
            pboxout->x1 = x1 = pbox->x1 + x;
            pboxout->y1 = y1 = pbox->y1 + y;
            pboxout->x2 = x2 = pbox->x2 + x;
            pboxout->y2 = y2 = pbox->y2 + y;
            if (((x2 - SHRT_MIN) | (y2 - SHRT_MIN) | (SHRT_MAX - x1) | (SHRT_MAX - y1)) <= 0)
            {
                --pReg->data->numRects;
                continue;
            }
            if (x1 < SHRT_MIN)
                pboxout->x1 = SHRT_MIN;
            else if (x2 > SHRT_MAX)
                pboxout->x2 = SHRT_MAX;
            if (y1 < SHRT_MIN)
                pboxout->y1 = SHRT_MIN;
            else if (y2 > SHRT_MAX)
                pboxout->y2 = SHRT_MAX;
            ++pboxout;
        }

        if (pboxout != pbox)
        {
            if (pReg->data->numRects == 1)
            {
                pReg->extents = *REGION_BOXPTR(pReg);
                freeData(pReg);
                pReg->data = nullptr;
            }
            else
                miSetExtents(pReg);
        }
    }
}

bool miRegionDataCopy(RegionPtr dst, RegionPtr src)
{
    good(dst);
    good(src);

    if (dst->data)
        return true;

    if (dst == src)
        return true;

    if (!src->data || !src->data->size)
    {
        freeData(dst);
        dst->data = nullptr;
        return true;
    }

    if (!dst->data || (dst->data->size < src->data->numRects))
    {
        freeData(dst);
        dst->data = mallocData(src->data->numRects);

        if (!dst->data)
            return miRegionBreak(dst);
    }

    dst->data->size = src->data->size;
    dst->data->numRects = src->data->numRects;

    return true;
}

void miRegionReset(RegionPtr pReg, BoxPtr pBox)
{
    good(pReg);
    assert(pBox->x1 <= pBox->x2);
    assert(pBox->y1 <= pBox->y2);
    pReg->extents = *pBox;
    freeData(pReg);
    pReg->data = nullptr;
}

bool miPointInRegion(RegionPtr pReg, int x, int y, BoxPtr box)
{
    good(pReg);

    int numRects = REGION_NUM_RECTS(pReg);
    if (!numRects || !INBOX(&pReg->extents, x, y))
        return false;

    if (numRects == 1)
    {
        *box = pReg->extents;
        return true;
    }

    BoxPtr pbox;
    BoxPtr pboxEnd;

    for (pbox = REGION_BOXPTR(pReg), pboxEnd = pbox + numRects;
         pbox != pboxEnd;
         pbox++)
    {
        if (y >= pbox->y2)
            continue;        /* not there yet */
        if ((y < pbox->y1) || (x < pbox->x1))
            break;        /* missed it */
        if (x >= pbox->x2)
            continue;        /* not there yet */
        *box = *pbox;
        return true;
    }

    return false;
}

bool miRegionNotEmpty(RegionPtr pReg)
{
    good(pReg);
    return (!REGION_NIL(pReg));
}

bool miRegionBroken(RegionPtr pReg)
{
    good(pReg);
    return (REGION_NAR(pReg));
}

void miRegionEmpty(RegionPtr pReg)
{
    good(pReg);
    freeData(pReg);
    pReg->extents.x2 = pReg->extents.x1;
    pReg->extents.y2 = pReg->extents.y1;
    pReg->data = &miEmptyData;
}

BoxPtr miRegionExtents(RegionPtr pReg)
{
    good(pReg);
    return (&pReg->extents);
}

#define ExchangeSpans(a, b)                                             \
{                                                                       \
    DDXPointRec tpt = spans[a]; spans[a] = spans[b]; spans[b] = tpt;    \
    int tw = widths[a]; widths[a] = widths[b]; widths[b] = tw;          \
}

/* ||| I should apply the merge sort code to rectangle sorting above, and see
   if mapping time can be improved.  But right now I've been at work 12 hours,
   so forget it.
*/

static void QuickSortSpans(DDXPointRec spans[], int widths[], int numSpans)
{
    int y;
    int i, j, m;
    DDXPointPtr r;

    /* Always called with numSpans > 1 */
    /* Sorts only by y, doesn't bother to sort by x */

    do
    {
        if (numSpans < 9)
        {
            /* Do insertion sort */
            int yprev;

            yprev = spans[0].y;
            i = 1;
            do
            { /* while i != numSpans */
                y = spans[i].y;
                if (yprev > y)
                {
                    /* spans[i] is out of order.  Move into proper location. */
                    DDXPointRec tpt;
                    int tw, k;

                    for (j = 0; y >= spans[j].y; j++) {}
                    tpt = spans[i];
                    tw  = widths[i];
                    for (k = i; k != j; k--)
                    {
                        spans[k] = spans[k-1];
                        widths[k] = widths[k-1];
                    }
                    spans[j] = tpt;
                    widths[j] = tw;
                    y = spans[i].y;
                } /* if out of order */
                yprev = y;
                ++i;
            }
            while (i != numSpans);

            return;
        }

        /* Choose partition element, stick in location 0 */
        m = numSpans / 2;
        if (spans[m].y > spans[0].y)
            ExchangeSpans(m, 0);

        if (spans[m].y > spans[numSpans-1].y)
            ExchangeSpans(m, numSpans-1);

        if (spans[m].y > spans[0].y)
            ExchangeSpans(m, 0);

        y = spans[0].y;

        // Partition array
        i = 0;
        j = numSpans;

        do
        {
            r = &(spans[i]);
            do
            {
                ++r;
                ++i;
            }
            while (i != numSpans && r->y < y);

            r = &(spans[j]);

            do
            {
                --r;
                --j;
            }
            while (y < r->y);

            if (i < j)
                ExchangeSpans(i, j);
        }
        while (i < j);

        // Move partition element back to middle
        ExchangeSpans(0, j);

        // Recurse
        if (numSpans - j - 1 > 1)
            QuickSortSpans(&spans[j + 1], &widths[j + 1], numSpans - j - 1);

        numSpans = j;
    }
    while (numSpans > 1);
}

#define NextBand()                                                \
{                                                                 \
    clipy1 = pboxBandStart->y1;                                   \
    clipy2 = pboxBandStart->y2;                                   \
    pboxBandEnd = pboxBandStart + 1;                              \
    while (pboxBandEnd != pboxLast && pboxBandEnd->y1 == clipy1)  \
    {                                                             \
        ++pboxBandEnd;                                            \
    }                                                             \
    for (; ppt != pptLast && ppt->y < clipy1; ppt++, pwidth++) {} \
}

/*
    Clip a list of scanlines to a region.  The caller has allocated the
    space.  FSorted is non-zero if the scanline origins are in ascending
    order.
    returns the number of new, clipped scanlines.
*/

int miClipSpans(RegionPtr prgnDst,
                DDXPointPtr ppt,
                int* pwidth,
                int nspans,
                DDXPointPtr pptNew,
                int* pwidthNew,
                int fSorted)
{
    int y, x1, x2;
    int numRects;

    good(prgnDst);

    DDXPointPtr pptLast = ppt + nspans;
    int* pwidthNewStart = pwidthNew;

    if (!prgnDst->data)
    {
        // Do special fast code with clip boundaries in registers(?)
        // It doesn't pay much to make use of fSorted in this case,
        //   so we lump everything together.

        int clipx1, clipx2, clipy1, clipy2;

        clipx1 = prgnDst->extents.x1;
        clipy1 = prgnDst->extents.y1;
        clipx2 = prgnDst->extents.x2;
        clipy2 = prgnDst->extents.y2;
        
        for (; ppt != pptLast; ppt++, pwidth++)
        {
            y = ppt->y;
            x1 = ppt->x;

            if (clipy1 <= y && y < clipy2)
            {
                x2 = x1 + *pwidth;
                if (x1 < clipx1)
                    x1 = clipx1;

                if (x2 > clipx2)
                    x2 = clipx2;

                if (x1 < x2)
                {
                    // part of span in clip rectangle
                    pptNew->x = x1;
                    pptNew->y = y;
                    *pwidthNew = x2 - x1;
                    ++pptNew;
                    ++pwidthNew;
                }
            }
        }
    }
    else if ((numRects = prgnDst->data->numRects))
    {
        // Have to clip against many boxes
        BoxPtr pboxBandEnd;
        int clipy1, clipy2;

        // In this case, taking advantage of sorted spans gains more than
        // the sorting costs.
        if (!fSorted && nspans > 1)
            QuickSortSpans(ppt, pwidth, nspans);

        BoxPtr pboxBandStart = REGION_BOXPTR(prgnDst);
        BoxPtr pboxLast = pboxBandStart + numRects;
    
        NextBand();

        for (; ppt != pptLast; )
        {
            y = ppt->y;

            if (y < clipy2)
            {
                // span is in the current band
                BoxPtr pbox = pboxBandStart;
                x1 = ppt->x;
                x2 = x1 + *pwidth;

                do
                {
                    int newx1, newx2;

                    newx1 = x1;
                    newx2 = x2;

                    if (newx1 < pbox->x1)
                        newx1 = pbox->x1;

                    if (newx2 > pbox->x2)
                        newx2 = pbox->x2;

                    if (newx1 < newx2)
                    {
                        // Part of span in clip rectangle
                        pptNew->x = newx1;
                        pptNew->y = y;
                        *pwidthNew = newx2 - newx1;

                        ++pptNew;
                        ++pwidthNew;
                    }

                    ++pbox;
                }
                while (pbox != pboxBandEnd);

                ++ppt;
                ++pwidth;
            }
            else
            {
                // Move to next band, adjust ppt as needed
                pboxBandStart = pboxBandEnd;

                if (pboxBandStart == pboxLast)
                    break; // We're completely done

                NextBand();
            }
        }
    }

    return pwidthNew - pwidthNewStart;
}

// find the band in a region with the most rectangles
int miFindMaxBand(RegionPtr prgn)
{
    int nMaxBand = 0;

    good(prgn);

    int nbox = REGION_NUM_RECTS(prgn);
    BoxPtr pbox = REGION_RECTS(prgn);

    while (nbox > 0)
    {
        short yThisBand = pbox->y1;
        int nThisBand = 0;

        while ((nbox > 0) && (pbox->y1 == yThisBand))
        {
            --nbox;
            ++pbox;
            ++nThisBand;
        }

        if (nThisBand > nMaxBand)
            nMaxBand = nThisBand;
    }

    return nMaxBand;
}
