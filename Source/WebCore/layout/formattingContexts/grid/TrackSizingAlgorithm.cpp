/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TrackSizingAlgorithm.h"

#include "LayoutIntegrationUtils.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "TrackSizingFunctions.h"
#include <wtf/Range.h>
#include <wtf/Vector.h>
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace Layout {

struct FlexTrack {
    size_t trackIndex;
    Style::GridTrackBreadth::Flex flexFactor;
    LayoutUnit baseSize;
    LayoutUnit growthLimit;

    constexpr FlexTrack(size_t index, Style::GridTrackBreadth::Flex factor, LayoutUnit base, LayoutUnit growth)
        : trackIndex(index)
        , flexFactor(factor)
        , baseSize(base)
        , growthLimit(growth)
    {
    }
};

struct UnsizedTrack {
    LayoutUnit baseSize;
    LayoutUnit growthLimit;
    const TrackSizingFunctions trackSizingFunction;
};


using GridItemIndexes = Vector<size_t>;
static GridItemIndexes singleSpanningItemsWithinTrack(size_t trackIndex, const PlacedGridItemSpanList& gridItemSpanList)
{
    GridItemIndexes nonSpanningItems;
    for (auto [gridItemIndex, gridItemSpan] : WTF::indexedRange(gridItemSpanList)) {
        if (gridItemSpan.distance() == 1 && gridItemSpan.begin() == trackIndex)
            nonSpanningItems.append(gridItemIndex);
    }
    return nonSpanningItems;
}

using TrackIndexes = Vector<size_t>;
static TrackIndexes tracksWithIntrinsicSizingFunction(const UnsizedTracks& unsizedTracks)
{
    TrackIndexes trackList;
    for (auto [trackIndex, track] : WTF::indexedRange(unsizedTracks)) {
        auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
        auto& maximumTrackSizingFunction = track.trackSizingFunction.max;
        if (minimumTrackSizingFunction.isFlex() || maximumTrackSizingFunction.isFlex())
            continue;

        if (minimumTrackSizingFunction.isContentSized() || maximumTrackSizingFunction.isContentSized())
            trackList.append(trackIndex);
    }
    return trackList;
}

static Vector<LayoutUnit> minContentContributions(const PlacedGridItems&, const GridItemIndexes&, const IntegrationUtils&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { { } };
}

static void sizeTracksToFitNonSpanningItems(UnsizedTracks& unsizedTracks, const PlacedGridItems& gridItems, const PlacedGridItemSpanList& gridItemSpanList,
    const IntegrationUtils& integrationUtils)
{
    // For each track with an intrinsic track sizing function and not a flexible sizing function, consider the items in it with a span of 1:
    for (auto trackIndex : tracksWithIntrinsicSizingFunction(unsizedTracks)) {
        auto& track = unsizedTracks[trackIndex];
        auto singleSpanningItems = singleSpanningItemsWithinTrack(trackIndex, gridItemSpanList);

        auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
        track.baseSize = WTF::switchOn(minimumTrackSizingFunction,
            [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                // If the track has a min-content min track sizing function, set its base size
                // to the maximum of the items’ min-content contributions, floored at zero.
                auto itemContributions = minContentContributions(gridItems, singleSpanningItems, integrationUtils);
                ASSERT(itemContributions.size() == singleSpanningItems.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::max({ }, std::ranges::max(itemContributions));
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const auto&) -> LayoutUnit {
                ASSERT_NOT_REACHED();
                return { };
            }
        );

        auto& maximumTrackSizingFunction = track.trackSizingFunction.max;
        track.growthLimit = WTF::switchOn(maximumTrackSizingFunction,
            [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                // If the track has a min-content max track sizing function, set its growth
                // limit to the maximum of the items’ min-content contributions.
                auto itemContributions = minContentContributions(gridItems, singleSpanningItems, integrationUtils);
                ASSERT(itemContributions.size() == singleSpanningItems.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::ranges::max(itemContributions);
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const auto&) -> LayoutUnit {
                ASSERT_NOT_REACHED();
                return { };
            }
        );
    }
}

// https://drafts.csswg.org/css-grid-1/#algo-content
static void resolveIntrinsicTrackSizes(UnsizedTracks& unsizedTracks, const PlacedGridItems& gridItems, const PlacedGridItemSpanList& gridItemSpanList,
    const IntegrationUtils& integrationUtils)
{
    // 1. Shim baseline-aligned items so their intrinsic size contributions reflect their
    // baseline alignment.
    auto shimBaselineAlignedItems = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(shimBaselineAlignedItems);

    // 2. Size tracks to fit non-spanning items.
    sizeTracksToFitNonSpanningItems(unsizedTracks, gridItems, gridItemSpanList, integrationUtils);

    // 3. Increase sizes to accommodate spanning items crossing content-sized tracks:
    // Next, consider the items with a span of 2 that do not span a track with a flexible
    // sizing function.
    auto increaseSizesToAccommodateSpanningItemsCrossingContentSizedTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(increaseSizesToAccommodateSpanningItemsCrossingContentSizedTracks);

    // 4. Increase sizes to accommodate spanning items crossing flexible tracks:
    auto increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks);

    // 5. If any track still has an infinite growth limit, set its growth limit to its base size.
    auto setInfiniteGrowthLimitsToBaseSize = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(setInfiniteGrowthLimitsToBaseSize);
}

// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
TrackSizes TrackSizingAlgorithm::sizeTracks(const PlacedGridItems& gridItems, const PlacedGridItemSpanList& gridItemSpanList, const TrackSizingFunctionsList& trackSizingFunctions, std::optional<LayoutUnit> availableSpace, const IntegrationUtils& integrationUtils)
{
    ASSERT(gridItems.size() == gridItemSpanList.size());

    // 1. Initialize Track Sizes
    auto unsizedTracks = initializeTrackSizes(trackSizingFunctions);

    // 2. Resolve Intrinsic Track Sizes
    resolveIntrinsicTrackSizes(unsizedTracks, gridItems, gridItemSpanList, integrationUtils);

    // 3. Maximize Tracks
    auto maximizeTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(maximizeTracks);

    // 4. Expand Flexible Tracks
    auto expandFlexibleTracks = [&] {
        if (!hasFlexTracks(unsizedTracks))
            return;
        auto flexTracks = collectFlexTracks(unsizedTracks);
        double totalFlex = flexFactorSum(flexTracks);
        if (!totalFlex)
            return;

        auto space = leftoverSpace(availableSpace, unsizedTracks);
        if (!space)
            return;

        // https://drafts.csswg.org/css-grid-1/#typedef-flex
        // FIXME: Handle the case where the total flex factor is less than 1fr.
        if (totalFlex < 1.0) {
            ASSERT_NOT_IMPLEMENTED_YET();
            return;
        }

        // FIXME: finish implementation for flex track sizing.
        auto frSize = space.value() / LayoutUnit { totalFlex };
        UNUSED_VARIABLE(frSize);
    };
    UNUSED_VARIABLE(expandFlexibleTracks);

    // 5. Expand Stretched auto Tracks
    auto expandStretchedAutoTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(expandStretchedAutoTracks);

    // Each track has a base size, a <length> which grows throughout the algorithm and
    // which will eventually be the track’s final size...
    return unsizedTracks.map([](const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize;
    });
}

// https://www.w3.org/TR/css-grid-1/#algo-init
UnsizedTracks TrackSizingAlgorithm::initializeTrackSizes(const TrackSizingFunctionsList& trackSizingFunctionsList)
{
    return trackSizingFunctionsList.map([](const TrackSizingFunctions& trackSizingFunctions) -> UnsizedTrack {
        // For each track, if the track’s min track sizing function is:
        auto baseSize = [&] -> LayoutUnit {
            auto& minTrackSizingFunction = trackSizingFunctions.min;

            // A fixed sizing function
            // Resolve to an absolute length and use that size as the track’s initial base size.
            if (minTrackSizingFunction.isLength()) {
                auto& trackBreadthLength = minTrackSizingFunction.length();
                if (auto fixedValue = trackBreadthLength.tryFixed())
                    return LayoutUnit { fixedValue->resolveZoom(Style::ZoomNeeded { }) };

                if (auto percentValue = trackBreadthLength.tryPercentage()) {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return { };
                }

            }

            // An intrinsic sizing function
            // Use an initial base size of zero.
            if (minTrackSizingFunction.isContentSized())
                return { };

            ASSERT_NOT_REACHED();
            return { };
        };

        // For each track, if the track’s max track sizing function is:
        auto growthLimit = [&] -> LayoutUnit {
            auto& maxTrackSizingFunction = trackSizingFunctions.max;

            // A fixed sizing function
            // Resolve to an absolute length and use that size as the track’s initial growth limit.
            if (maxTrackSizingFunction.isLength()) {
                auto trackBreadthLength = maxTrackSizingFunction.length();
                if (auto fixedValue = trackBreadthLength.tryFixed())
                    return LayoutUnit { fixedValue->resolveZoom(Style::ZoomNeeded { }) };
            }

            // An intrinsic sizing function
            // A flexible sizing function
            // Use an initial growth limit of infinity.
            if (maxTrackSizingFunction.isContentSized() || maxTrackSizingFunction.isFlex())
                return LayoutUnit::max();

            ASSERT_NOT_REACHED();
            return { };
        };

        return { baseSize(), growthLimit(), trackSizingFunctions };
    });
}

FlexTracks TrackSizingAlgorithm::collectFlexTracks(const UnsizedTracks& unsizedTracks)
{
    FlexTracks flexTracks;

    for (auto [trackIndex, track] : indexedRange(unsizedTracks)) {
        const auto& maxTrackSizingFunction = track.trackSizingFunction.max;

        if (maxTrackSizingFunction.isFlex()) {
            auto flexFactor = maxTrackSizingFunction.flex();
            flexTracks.append(FlexTrack(trackIndex, flexFactor, track.baseSize, track.growthLimit));
        }
    }

    return flexTracks;
}

bool TrackSizingAlgorithm::hasFlexTracks(const UnsizedTracks& unsizedTracks)
{
    return std::ranges::any_of(unsizedTracks, [](auto& track) {
        return track.trackSizingFunction.max.isFlex();
    });
}

double TrackSizingAlgorithm::flexFactorSum(const FlexTracks& flexTracks)
{
    double total = 0.0;
    for (auto& track : flexTracks)
        total += track.flexFactor.value;
    return total;
}

// https://drafts.csswg.org/css-grid-1/#algo-find-fr-size
// https://drafts.csswg.org/css-grid-1/#leftover-space
std::optional<LayoutUnit> TrackSizingAlgorithm::leftoverSpace(std::optional<LayoutUnit> availableSpace, const UnsizedTracks& unsizedTracks)
{
    if (!availableSpace)
        return std::nullopt;

    // Sum only non-flexible tracks. Flexible tracks are being sized, so their base sizes don't count against available space.
    // FIXME: This doesn't implement step 4 of "Find the Size of an fr" where some flex tracks should be treated as inflexible.
    LayoutUnit usedSpace;
    for (const auto& track : unsizedTracks) {
        if (!track.trackSizingFunction.max.isFlex())
            usedSpace += track.baseSize;
    }

    auto leftoverSpace = availableSpace.value() - usedSpace;
    if (leftoverSpace <= 0_lu)
        return { 0 };

    return leftoverSpace;
}

} // namespace Layout
} // namespace WebCore
