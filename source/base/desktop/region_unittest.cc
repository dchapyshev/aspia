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

#include "base/desktop/region.h"

#include <gtest/gtest.h>

#include <QRegion>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "base/time_types.h"

namespace {

// Extracts the rectangles produced by iterating a region (Region or QRegion).
template <typename RegionType>
std::vector<QRect> rectsOf(const RegionType& region)
{
    std::vector<QRect> result;
    for (const QRect& rect : region)
        result.push_back(rect);
    return result;
}

//--------------------------------------------------------------------------------------------------
QRegion toQRegion(const std::vector<QRect>& rects)
{
    QRegion region;
    for (const QRect& rect : rects)
        region += rect;
    return region;
}

//--------------------------------------------------------------------------------------------------
qint64 totalArea(const std::vector<QRect>& rects)
{
    qint64 area = 0;
    for (const QRect& rect : rects)
        area += static_cast<qint64>(rect.width()) * rect.height();
    return area;
}

// Verifies the structural invariants of the canonical y-banded form produced by a region: no empty
// rectangles, ordered top-to-bottom then left-to-right, pairwise disjoint, and no two spans left
// unmerged inside the same band (same top and bottom, touching or overlapping horizontally). Note
// that two vertically adjacent rectangles with identical horizontal extent are NOT necessarily a
// missed merge: in banded form a column is legitimately split wherever a neighbouring span starts
// or ends, exactly as QRegion does. The expensive pairwise checks are skipped for very large
// outputs - disjointness there is covered by the area comparison in expectEquivalent().
void checkCanonical(const std::vector<QRect>& rects)
{
    for (const QRect& rect : rects)
        EXPECT_FALSE(rect.isEmpty()) << "empty rectangle in canonical form";

    for (size_t i = 1; i < rects.size(); ++i)
    {
        const QRect& a = rects[i - 1];
        const QRect& b = rects[i];
        const bool ordered = a.y() < b.y() || (a.y() == b.y() && a.x() < b.x());
        EXPECT_TRUE(ordered) << "rectangles not in canonical order at index " << i;
    }

    if (rects.size() > 200)
        return;

    for (size_t i = 0; i < rects.size(); ++i)
    {
        for (size_t j = i + 1; j < rects.size(); ++j)
        {
            const QRect& a = rects[i];
            const QRect& b = rects[j];

            EXPECT_FALSE(a.intersects(b)) << "overlapping rectangles " << i << " and " << j;

            const bool horizontal_merge_missed =
                a.top() == b.top() && a.bottom() == b.bottom() &&
                (a.right() + 1 == b.left() || b.right() + 1 == a.left());
            EXPECT_FALSE(horizontal_merge_missed) << "unmerged spans inside one band";
        }
    }
}

// Checks that |region| covers exactly the same geometry as the QRegion |oracle| and that its
// canonical decomposition is well formed and disjoint (the latter follows from the area equality
// because the oracle decomposition is always disjoint).
template <typename RegionType>
void expectEquivalent(const RegionType& region, const QRegion& oracle)
{
    const std::vector<QRect> mine = rectsOf(region);
    checkCanonical(mine);

    const QRegion mine_region = toQRegion(mine);
    EXPECT_TRUE(mine_region.subtracted(oracle).isEmpty()) << "region covers area outside oracle";
    EXPECT_TRUE(oracle.subtracted(mine_region).isEmpty()) << "region misses area of oracle";
    EXPECT_EQ(totalArea(mine), totalArea(rectsOf(oracle))) << "rectangles overlap or miss area";
}

void expectMatchesOracle(const std::vector<QRect>& input)
{
    QRegion oracle;
    Region region;

    for (const QRect& rect : input)
    {
        oracle += rect;
        region += rect;
    }

    expectEquivalent(region, oracle);
}

// A grid of disjoint blocks in row-major (scanline) order, the way Differ::mergeBlocks visits the
// diff grid. With |density| < 1 some blocks are skipped, producing a sparse pattern.
std::vector<QRect> scanlineBlocks(std::mt19937& rng, int cols, int rows, int block, double density)
{
    std::vector<QRect> rects;
    std::uniform_real_distribution<double> chance(0.0, 1.0);

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            if (chance(rng) < density)
                rects.emplace_back(x * block, y * block, block, block);
        }
    }

    return rects;
}

// Arbitrarily sized rectangles scattered over the screen, sorted into scanline order - close to
// what the DXGI duplicator and scaler feed into the region (a small number of already-merged,
// sometimes overlapping rectangles).
std::vector<QRect> scatteredRects(std::mt19937& rng, int count, int screen_w, int screen_h,
                                  int min_size, int max_size)
{
    std::uniform_int_distribution<int> size(min_size, max_size);

    std::vector<QRect> rects;
    rects.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        const int w = size(rng);
        const int h = size(rng);
        std::uniform_int_distribution<int> px(0, std::max(0, screen_w - w));
        std::uniform_int_distribution<int> py(0, std::max(0, screen_h - h));
        rects.emplace_back(px(rng), py(rng), w, h);
    }

    std::sort(rects.begin(), rects.end(), [](const QRect& a, const QRect& b)
    {
        return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
    });

    return rects;
}

// Grid-aligned merged rectangles in scanline order - what Differ::mergeBlocks actually produces
// (adjacent dirty 16px blocks already combined into larger rectangles), as opposed to thousands of
// individual blocks.
std::vector<QRect> mergedGridRects(std::mt19937& rng, int count, int cols, int rows, int block)
{
    std::uniform_int_distribution<int> width_blocks(1, 10);
    std::uniform_int_distribution<int> height_blocks(1, 3);

    std::vector<QRect> rects;
    rects.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        const int w = width_blocks(rng);
        const int h = height_blocks(rng);
        std::uniform_int_distribution<int> px(0, std::max(0, cols - w));
        std::uniform_int_distribution<int> py(0, std::max(0, rows - h));
        rects.emplace_back(px(rng) * block, py(rng) * block, w * block, h * block);
    }

    std::sort(rects.begin(), rects.end(), [](const QRect& a, const QRect& b)
    {
        return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
    });

    return rects;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, EmptyByDefault)
{
    Region region;
    EXPECT_TRUE(region.isEmpty());
    EXPECT_TRUE(rectsOf(region).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, EmptyRectIgnored)
{
    Region region;
    region += QRect(10, 10, 0, 0);
    region += QRect(20, 20, 5, 0);
    region += QRect(30, 30, 0, 5);
    EXPECT_TRUE(region.isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, SingleRect)
{
    expectMatchesOracle({ QRect(5, 7, 30, 20) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, DisjointRects)
{
    expectMatchesOracle({ QRect(0, 0, 10, 10), QRect(50, 50, 10, 10), QRect(100, 5, 20, 30) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, OverlappingRects)
{
    expectMatchesOracle({ QRect(0, 0, 40, 40), QRect(20, 20, 40, 40) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, FullyContainedRect)
{
    expectMatchesOracle({ QRect(0, 0, 100, 100), QRect(10, 10, 5, 5) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, IdenticalRects)
{
    expectMatchesOracle({ QRect(3, 4, 7, 8), QRect(3, 4, 7, 8), QRect(3, 4, 7, 8) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, HorizontalCoalesce)
{
    Region region;
    region += QRect(0, 0, 20, 20);
    region += QRect(20, 0, 20, 20);

    EXPECT_EQ(rectsOf(region), std::vector<QRect>{ QRect(0, 0, 40, 20) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, VerticalCoalesce)
{
    Region region;
    region += QRect(0, 0, 20, 20);
    region += QRect(0, 20, 20, 20);

    EXPECT_EQ(rectsOf(region), std::vector<QRect>{ QRect(0, 0, 20, 40) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, CrossShapeNotOverMerged)
{
    // A plus/cross shape cannot be a single rectangle; it decomposes into three banded rectangles.
    Region region;
    region += QRect(10, 0, 10, 30);  // vertical bar
    region += QRect(0, 10, 30, 10);  // horizontal bar

    QRegion oracle(QRect(10, 0, 10, 30));
    oracle += QRect(0, 10, 30, 10);

    expectEquivalent(region, oracle);
    EXPECT_EQ(rectsOf(region).size(), 3u);
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, NegativeCoordinates)
{
    expectMatchesOracle(
        { QRect(-50, -40, 30, 20), QRect(-10, -10, 100, 100), QRect(-100, 5, 20, 200) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, SinglePixelRects)
{
    expectMatchesOracle(
        { QRect(0, 0, 1, 1), QRect(1, 0, 1, 1), QRect(0, 1, 1, 1), QRect(5, 5, 1, 1) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, Grid)
{
    std::mt19937 rng(1);
    expectMatchesOracle(scanlineBlocks(rng, 20, 20, 16, 0.5));
}

//--------------------------------------------------------------------------------------------------
// Builds a region from far more rectangles than the internal compaction threshold so that the
// periodic (non-lazy) normalization path is exercised, and compares it with QRegion.
TEST(RegionTest, LargeUnionCrossesCompaction)
{
    std::mt19937 rng(99);
    expectMatchesOracle(scatteredRects(rng, 1500, 1280, 720, 8, 120));
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, UnionRegion)
{
    Region a;
    a += QRect(0, 0, 30, 30);
    a += QRect(60, 0, 30, 30);

    Region b;
    b += QRect(20, 10, 50, 10);

    QRegion oracle(QRect(0, 0, 30, 30));
    oracle += QRect(60, 0, 30, 30);
    oracle += QRect(20, 10, 50, 10);

    a += b;
    expectEquivalent(a, oracle);
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, SelfUnion)
{
    Region a;
    a += QRect(0, 0, 30, 30);
    a += QRect(40, 40, 10, 10);
    a += a;

    QRegion oracle(QRect(0, 0, 30, 30));
    oracle += QRect(40, 40, 10, 10);

    expectEquivalent(a, oracle);
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, IntersectRect)
{
    Region region;
    region += QRect(0, 0, 100, 100);
    region += QRect(150, 150, 50, 50);

    QRegion oracle(QRect(0, 0, 100, 100));
    oracle += QRect(150, 150, 50, 50);

    QRect clip(50, 50, 120, 120);
    expectEquivalent(region.intersected(clip), oracle.intersected(clip));
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, IntersectRectNoOverlap)
{
    Region region;
    region += QRect(0, 0, 10, 10);

    EXPECT_TRUE(region.intersected(QRect(100, 100, 10, 10)).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, IntersectInPlace)
{
    Region region;
    region += QRect(0, 0, 100, 100);
    region += QRect(150, 150, 50, 50);
    region.intersect(QRect(50, 50, 120, 120));

    QRegion oracle(QRect(0, 0, 100, 100));
    oracle += QRect(150, 150, 50, 50);

    expectEquivalent(region, oracle.intersected(QRect(50, 50, 120, 120)));

    // A clamp that does not cut anything must leave the region unchanged.
    Region whole;
    whole += QRect(10, 10, 30, 40);
    const std::vector<QRect> before = rectsOf(whole);
    whole.intersect(QRect(0, 0, 1000, 1000));
    EXPECT_EQ(rectsOf(whole), before);

    // A clamp that misses entirely empties the region.
    Region gone;
    gone += QRect(0, 0, 10, 10);
    gone.intersect(QRect(100, 100, 5, 5));
    EXPECT_TRUE(gone.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Exercises clear() + rebuild reuse, including multi-span rows (whose span buffers are pooled), to
// make sure a recycled buffer is reset and never carries stale spans.
TEST(RegionTest, ClearAndRebuild)
{
    Region region;
    region += QRect(0, 0, 30, 20);   // band [0,20): two disjoint spans -> multi-span row (heap)
    region += QRect(50, 0, 30, 20);
    region += QRect(0, 40, 200, 20);

    region.clear();
    EXPECT_TRUE(region.isEmpty());

    // Rebuild with a different shape; the pooled buffers must come back empty.
    region += QRect(10, 5, 20, 10);
    region += QRect(70, 5, 20, 10);
    region += QRect(100, 100, 40, 40);

    QRegion oracle(QRect(10, 5, 20, 10));
    oracle += QRect(70, 5, 20, 10);
    oracle += QRect(100, 100, 40, 40);

    expectEquivalent(region, oracle);

    region.clear();
    EXPECT_TRUE(region.isEmpty());
    EXPECT_TRUE(rectsOf(region).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, UnionIntoEmpty)
{
    Region a;
    Region b;
    b += QRect(0, 0, 30, 30);
    b += QRect(40, 40, 10, 10);

    a += b; // exercises the empty-LHS fast path

    QRegion oracle(QRect(0, 0, 30, 30));
    oracle += QRect(40, 40, 10, 10);

    expectEquivalent(a, oracle);
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, IntersectRegion)
{
    Region a;
    a += QRect(0, 0, 100, 60);

    Region b;
    b += QRect(40, 0, 100, 100);

    QRegion oracle_a(QRect(0, 0, 100, 60));
    QRegion oracle_b(QRect(40, 0, 100, 100));

    expectEquivalent(a.intersected(b), oracle_a.intersected(oracle_b));
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, Translate)
{
    Region region;
    region += QRect(0, 0, 20, 20);
    region += QRect(40, 40, 20, 20);
    region.translate(100, -10);

    QRegion oracle(QRect(0, 0, 20, 20));
    oracle += QRect(40, 40, 20, 20);
    oracle.translate(100, -10);

    expectEquivalent(region, oracle);
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, TranslateBeforeNormalize)
{
    // Translation must apply to not-yet-normalized rectangles too.
    Region region;
    region += QRect(0, 0, 10, 10);
    region += QRect(20, 0, 10, 10);
    region.translate(5, 5);

    QRegion oracle(QRect(0, 0, 10, 10));
    oracle += QRect(20, 0, 10, 10);
    oracle.translate(5, 5);

    expectEquivalent(region, oracle);
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, Contains)
{
    Region region;
    region += QRect(0, 0, 100, 100);
    region += QRect(100, 0, 50, 50); // touches the first rect, forms an L-shape

    QRegion oracle(QRect(0, 0, 100, 100));
    oracle += QRect(100, 0, 50, 50);

    // QRegion::contains(QRect) reports any overlap, not full containment - match it exactly.
    const QRect queries[] =
    {
        QRect(10, 10, 20, 20),    // fully inside the first rect
        QRect(0, 0, 100, 100),    // exactly the first rect
        QRect(90, 0, 20, 50),     // straddles both rectangles
        QRect(100, 50, 10, 10),   // just below the small rect - outside
        QRect(95, 95, 20, 20),    // pokes outside but overlaps the corner
        QRect(200, 200, 5, 5),    // completely outside
        QRect(140, 40, 20, 20),   // partially overlaps the small rect
    };

    for (const QRect& query : queries)
    {
        SCOPED_TRACE("query " + std::to_string(query.x()) + "," + std::to_string(query.y()));
        EXPECT_EQ(region.contains(query), oracle.contains(query));
    }
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, Swap)
{
    Region a;
    a += QRect(0, 0, 10, 10);

    Region b;
    b += QRect(50, 50, 10, 10);

    a.swap(b);

    EXPECT_EQ(rectsOf(a), std::vector<QRect>{ QRect(50, 50, 10, 10) });
    EXPECT_EQ(rectsOf(b), std::vector<QRect>{ QRect(0, 0, 10, 10) });
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, CopyAndMove)
{
    Region a;
    a += QRect(0, 0, 30, 20);
    a += QRect(40, 0, 10, 60);

    Region copy(a);
    EXPECT_EQ(rectsOf(copy), rectsOf(a));

    Region assigned;
    assigned += QRect(1000, 1000, 5, 5);
    assigned = a;
    EXPECT_EQ(rectsOf(assigned), rectsOf(a));

    const std::vector<QRect> expected = rectsOf(a);
    Region moved(std::move(a));
    EXPECT_EQ(rectsOf(moved), expected);
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, AssignRect)
{
    Region region;
    region += QRect(0, 0, 200, 200);
    region = QRect(10, 10, 5, 5);

    EXPECT_EQ(rectsOf(region), std::vector<QRect>{ QRect(10, 10, 5, 5) });

    region = QRect();
    EXPECT_TRUE(region.isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(RegionTest, IterationIsIdempotent)
{
    Region region;
    region += QRect(0, 0, 40, 40);
    region += QRect(20, 20, 40, 40);

    const std::vector<QRect> first = rectsOf(region);
    const std::vector<QRect> second = rectsOf(region);
    EXPECT_EQ(first, second);
}

//--------------------------------------------------------------------------------------------------
// Exhaustive differential test: a randomized sequence of every supported operation is applied in
// lockstep to a QRegion oracle and to the region under test. After each sequence the region must be
// geometrically equal to the oracle and its decomposition must be well formed.
template <typename RegionType>
void runDifferential(unsigned seed)
{
    std::mt19937 rng(seed);

    std::uniform_int_distribution<int> op_count(0, 25);
    std::uniform_int_distribution<int> op_kind(0, 3);
    std::uniform_int_distribution<int> small_count(1, 5);

    for (int trial = 0; trial < 3000; ++trial)
    {
        // Vary the coordinate window per trial so negative and larger coordinates are exercised.
        std::uniform_int_distribution<int> origin(-100, 100);
        const int base_x = origin(rng);
        const int base_y = origin(rng);
        std::uniform_int_distribution<int> offset(0, 150);
        std::uniform_int_distribution<int> extent(1, 80);

        auto randomRect = [&]() -> QRect
        {
            return QRect(base_x + offset(rng), base_y + offset(rng), extent(rng), extent(rng));
        };

        QRegion oracle;
        RegionType region;

        const int ops = op_count(rng);
        for (int o = 0; o < ops; ++o)
        {
            switch (op_kind(rng))
            {
                case 0: // add a single rectangle
                {
                    const QRect rect = randomRect();
                    oracle += rect;
                    region += rect;
                    break;
                }

                case 1: // union with another (small) region
                {
                    const int n = small_count(rng);
                    QRegion oracle_other;
                    RegionType region_other;

                    for (int k = 0; k < n; ++k)
                    {
                        const QRect rect = randomRect();
                        oracle_other += rect;
                        region_other += rect;
                    }

                    oracle += oracle_other;
                    region += region_other;
                    break;
                }

                case 2: // intersect with a clip rectangle (in place)
                {
                    const QRect clip = randomRect();
                    oracle = oracle.intersected(clip);
                    region.intersect(clip);
                    break;
                }

                case 3: // translate
                {
                    std::uniform_int_distribution<int> delta(-50, 50);
                    const int dx = delta(rng);
                    const int dy = delta(rng);
                    oracle.translate(dx, dy);
                    region.translate(dx, dy);
                    break;
                }
            }
        }

        SCOPED_TRACE("trial " + std::to_string(trial));
        ASSERT_EQ(region.isEmpty(), oracle.isEmpty());
        expectEquivalent(region, oracle);
    }
}

// Differential test for contains(): build a random region, then compare the answer for many query
// rectangles against QRegion::contains().
template <typename RegionType>
void runContains(unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> coord(0, 120);
    std::uniform_int_distribution<int> extent(1, 50);
    std::uniform_int_distribution<int> count(1, 15);

    for (int trial = 0; trial < 1000; ++trial)
    {
        QRegion oracle;
        RegionType region;

        const int n = count(rng);
        for (int k = 0; k < n; ++k)
        {
            const QRect rect(coord(rng), coord(rng), extent(rng), extent(rng));
            oracle += rect;
            region += rect;
        }

        for (int q = 0; q < 10; ++q)
        {
            const QRect query(coord(rng), coord(rng), extent(rng), extent(rng));
            SCOPED_TRACE("trial " + std::to_string(trial) + " query " + std::to_string(q));
            EXPECT_EQ(region.contains(query), oracle.contains(query));
        }
    }
}

//--------------------------------------------------------------------------------------------------
TEST(RegionFuzz, DifferentialAgainstQRegion) { runDifferential<Region>(0xA11CE); }
TEST(RegionFuzz, ContainsAgainstQRegion) { runContains<Region>(0xC0FFEE); }

namespace {

// Runs |body| in a calibrated loop: the iteration count is doubled until a batch runs for at least
// 40 ms, then the average time of that batch is returned in microseconds. |body| receives the
// iteration index and must return a value that depends on the work so the optimizer cannot elide
// it.
template <typename Body>
double timePerOpUs(Body&& body)
{
    constexpr double kMinBatchNs = 40'000'000.0;
    constexpr int kMaxIterations = 1 << 26;

    // Warm up caches and branch predictors once.
    volatile qint64 warm = body(0);
    (void)warm;

    int iterations = 64;
    for (;;)
    {
        const auto start = Clock::now();
        qint64 sink = 0;
        for (int i = 0; i < iterations; ++i)
            sink += body(i);
        const auto end = Clock::now();

        volatile qint64 keep = sink;
        (void)keep;

        const double ns = static_cast<double>(DurationCast<NanoSeconds>(end - start).count());

        if (ns >= kMinBatchNs || iterations >= kMaxIterations)
            return ns / iterations / 1000.0;

        iterations *= 2;
    }
}

template <typename RegionType>
qint64 consume(const RegionType& region)
{
    qint64 area = 0;
    for (const QRect& rect : region)
        area += static_cast<qint64>(rect.width()) * rect.height();
    return area;
}

template <typename RegionType>
double benchBuildConsume(const std::vector<QRect>& rects)
{
    return timePerOpUs([&](int) -> qint64
    {
        RegionType region;
        for (const QRect& rect : rects)
            region += rect;
        return consume(region);
    });
}

// Same build+consume, but the region object is reused across iterations and reset with clear() -
// modelling a region member that lives across frames (the row buffer is not reallocated each time).
double benchReuseConsume(const std::vector<QRect>& rects)
{
    Region region;
    return timePerOpUs([&](int) -> qint64
    {
        region.clear();
        for (const QRect& rect : rects)
            region += rect;
        return consume(region);
    });
}

// In-place clip, matching how the pipeline actually clamps a region to bounds. QRegion exposes
// operator&=(QRect); our Region exposes intersect(QRect).
void intersectInPlace(QRegion& region, const QRect& clip) { region &= clip; }
void intersectInPlace(Region& region, const QRect& clip) { region.intersect(clip); }

template <typename RegionType>
double benchBuildIntersect(const std::vector<QRect>& rects, const QRect& clip)
{
    return timePerOpUs([&](int) -> qint64
    {
        RegionType region;
        for (const QRect& rect : rects)
            region += rect;
        intersectInPlace(region, clip);
        return consume(region);
    });
}

template <typename RegionType>
double benchBuildTranslate(const std::vector<QRect>& rects)
{
    return timePerOpUs([&](int) -> qint64
    {
        RegionType region;
        for (const QRect& rect : rects)
            region += rect;
        region.translate(7, 13);
        return consume(region);
    });
}

void printRow(const std::string& label, double qregion_us, double region_us)
{
    std::cout << "    " << std::left << std::setw(26) << label << std::right
              << " QRegion " << std::setw(10) << std::fixed << std::setprecision(3) << qregion_us
              << " us   Region " << std::setw(10) << region_us
              << " us   speedup " << std::setprecision(2) << (qregion_us / region_us) << "x"
              << std::endl;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Not a pass/fail test: prints per-operation timings of Region against QRegion across a range of
// region sizes, for both scattered input (typical of DXGI/scaler/uneven diffs) and grid-aligned
// scanline input (QRegion's favourable case). The build+consume row is the metric that matches how
// the capture/codec pipeline actually uses regions.
TEST(RegionBenchmark, Operations)
{
    std::mt19937 rng(7);
    // Clamp to the full screen bounds: the region is already inside, so this is the no-op clamp that
    // the encoder/duplicator/scaler hit every frame (clip to image/desktop rect).
    const QRect clip(0, 0, 1920, 1080);

    const int sizes[] = { 16, 64, 256, 1024, 4096 };

    std::cout << "=== scattered input (random position and size) ===" << std::endl;
    for (int n : sizes)
    {
        const std::vector<QRect> rects = scatteredRects(rng, n, 1920, 1080, 8, 200);
        Region probe;
        for (const QRect& rect : rects)
            probe += rect;

        std::cout << "  n=" << n << " input rects, " << rectsOf(probe).size()
                  << " canonical rects" << std::endl;
        printRow("build+consume", benchBuildConsume<QRegion>(rects),
                 benchBuildConsume<Region>(rects));
        printRow("build+clamp+consume", benchBuildIntersect<QRegion>(rects, clip),
                 benchBuildIntersect<Region>(rects, clip));
        printRow("build+translate+consume", benchBuildTranslate<QRegion>(rects),
                 benchBuildTranslate<Region>(rects));
    }

    std::cout << "=== grid-aligned scanline input (16px blocks) ===" << std::endl;
    for (int n : sizes)
    {
        // Choose a grid that yields roughly |n| blocks at full density.
        const int cols = 120;
        const int rows = std::max(1, n / cols + 1);
        const double density = std::min(1.0, static_cast<double>(n) / (cols * rows));
        const std::vector<QRect> rects = scanlineBlocks(rng, cols, rows, 16, density);

        std::cout << "  ~" << rects.size() << " input blocks" << std::endl;
        printRow("build+consume", benchBuildConsume<QRegion>(rects),
                 benchBuildConsume<Region>(rects));
    }

    std::cout << "=== grid-aligned merged rects (differ-like) ===" << std::endl;
    for (int n : sizes)
    {
        const std::vector<QRect> rects = mergedGridRects(rng, n, 120, 68, 16);
        printRow("build+consume", benchBuildConsume<QRegion>(rects),
                 benchBuildConsume<Region>(rects));
        printRow("build+clamp+consume", benchBuildIntersect<QRegion>(rects, clip),
                 benchBuildIntersect<Region>(rects, clip));
    }

    std::cout << "=== Region reuse: fresh object vs clear() across frames ===" << std::endl;
    for (int n : sizes)
    {
        const std::vector<QRect> rects = scatteredRects(rng, n, 1920, 1080, 8, 200);
        const double fresh = benchBuildConsume<Region>(rects);
        const double reuse = benchReuseConsume(rects);
        std::cout << "    n=" << std::setw(5) << n << "   fresh "
                  << std::setw(9) << std::fixed << std::setprecision(3) << fresh
                  << " us   clear-reuse " << std::setw(9) << reuse
                  << " us   (" << std::setprecision(2) << (fresh / reuse) << "x)" << std::endl;
    }
}
