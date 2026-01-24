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
#include "GridLayoutUtils.h"

namespace WebCore {
namespace Layout {
namespace GridLayoutUtils {

LayoutUnit computeGapValue(const Style::GapGutter& gap)
{
    if (gap.isNormal())
        return { };

    // Only handle fixed length gaps for now
    if (auto fixedGap = gap.tryFixed())
        return Style::evaluate<LayoutUnit>(*fixedGap, 0_lu, Style::ZoomNeeded { });

    ASSERT_NOT_REACHED();
    return { };
}

LayoutUnit usedInlineSizeForGridItem(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit columnsSize)
{
    auto& inlineAxisSizes = placedGridItem.inlineAxisSizes();
    ASSERT(inlineAxisSizes.minimumSize.isFixed() && (inlineAxisSizes.maximumSize.isFixed() || inlineAxisSizes.maximumSize.isNone()));

    auto& preferredSize = inlineAxisSizes.preferredSize;
    if (auto fixedInlineSize = preferredSize.tryFixed())
        return LayoutUnit { fixedInlineSize->resolveZoom(placedGridItem.usedZoom()) } + borderAndPadding;

    if (preferredSize.isAuto()) {
        // Grid item calculations for automatic sizes in a given dimensions vary by their
        // self-alignment values:
        auto alignmentPosition = placedGridItem.inlineAxisAlignment().position();

        // normal:
        // If the grid item has no preferred aspect ratio, and no natural size in the relevant
        // axis (if it is a replaced element), the grid item is sized as for align-self: stretch.
        //
        // https://www.w3.org/TR/css-align-3/#propdef-align-self
        //
        // When the box’s computed width/height (as appropriate to the axis) is auto and neither of
        // its margins (in the appropriate axis) are auto, sets the box’s used size to the length
        // necessary to make its outer size as close to filling the alignment container as possible
        // while still respecting the constraints imposed by min-height/min-width/max-height/max-width.
        auto& marginStart = inlineAxisSizes.marginStart;
        auto& marginEnd = inlineAxisSizes.marginEnd;
        if ((alignmentPosition == ItemPosition::Normal) && !placedGridItem.hasPreferredAspectRatio() && !placedGridItem.isReplacedElement()
            && !marginStart.isAuto() && !marginEnd.isAuto()) {
            auto& usedZoom = placedGridItem.usedZoom();

            auto minimumSize = LayoutUnit { inlineAxisSizes.minimumSize.tryFixed()->resolveZoom(usedZoom) };
            auto maximumSize = [&inlineAxisSizes, &usedZoom] {
                auto& computedMaximumSize = inlineAxisSizes.maximumSize;
                if (computedMaximumSize.isNone())
                    return LayoutUnit::max();
                return LayoutUnit { computedMaximumSize.tryFixed()->resolveZoom(usedZoom) };
            };

            auto stretchedWidth = columnsSize - LayoutUnit { marginStart.tryFixed()->resolveZoom(usedZoom) } - LayoutUnit { marginEnd.tryFixed()->resolveZoom(usedZoom) } - borderAndPadding;
            return std::max(minimumSize, std::min(maximumSize(), stretchedWidth));
        }

        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

LayoutUnit usedBlockSizeForGridItem(const PlacedGridItem& placedGridItem, LayoutUnit borderAndPadding, LayoutUnit rowsSize)
{
    auto& blockAxisSizes = placedGridItem.blockAxisSizes();
    auto& preferredSize = blockAxisSizes.preferredSize;
    if (auto fixedBlockSize = preferredSize.tryFixed())
        return LayoutUnit { fixedBlockSize->resolveZoom(placedGridItem.usedZoom()) } + borderAndPadding;

    if (preferredSize.isAuto()) {
        // Grid item calculations for automatic sizes in a given dimensions vary by their
        // self-alignment values:
        auto alignmentPosition = placedGridItem.blockAxisAlignment().position();

        // normal:
        // If the grid item has no preferred aspect ratio, and no natural size in the relevant
        // axis (if it is a replaced element), the grid item is sized as for align-self: stretch.
        //
        // https://www.w3.org/TR/css-align-3/#propdef-align-self
        //
        // When the box’s computed width/height (as appropriate to the axis) is auto and neither of
        // its margins (in the appropriate axis) are auto, sets the box’s used size to the length
        // necessary to make its outer size as close to filling the alignment container as possible
        // while still respecting the constraints imposed by min-height/min-width/max-height/max-width.
        auto& marginStart = blockAxisSizes.marginStart;
        auto& marginEnd = blockAxisSizes.marginEnd;
        if ((alignmentPosition == ItemPosition::Normal) && !placedGridItem.hasPreferredAspectRatio() && !placedGridItem.isReplacedElement()
            && !marginStart.isAuto() && !marginEnd.isAuto()) {
            auto& usedZoom = placedGridItem.usedZoom();

            auto minimumSize = LayoutUnit { blockAxisSizes.minimumSize.tryFixed()->resolveZoom(usedZoom) };
            auto maximumSize = [&blockAxisSizes, &usedZoom] {
                auto& computedMaximumSize = blockAxisSizes.maximumSize;
                if (computedMaximumSize.isNone())
                    return LayoutUnit::max();
                return LayoutUnit { computedMaximumSize.tryFixed()->resolveZoom(usedZoom) };
            };
            auto stretchedBlockSize = rowsSize - LayoutUnit { marginStart.tryFixed()->resolveZoom(usedZoom) } - LayoutUnit { marginEnd.tryFixed()->resolveZoom(usedZoom) } - borderAndPadding;
            return std::max(minimumSize, std::min(maximumSize(), stretchedBlockSize));
        }
    }

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}


LayoutUnit computeGridLinePosition(size_t gridLineIndex, const TrackSizes& trackSizes, LayoutUnit gap)
{
    auto trackSizesBefore = trackSizes.subspan(0, gridLineIndex);
    auto sumOfTrackSizes = std::reduce(trackSizesBefore.begin(), trackSizesBefore.end());

    // For grid line i, there are i-1 gaps before it (between the i tracks)
    auto numberOfGaps = gridLineIndex > 0 ? gridLineIndex - 1 : 0;

    return sumOfTrackSizes + (numberOfGaps * gap);
}

LayoutUnit gridAreaDimensionSize(size_t startLine, size_t endLine, const TrackSizes& trackSizes, LayoutUnit gap)
{
    ASSERT(endLine > startLine);

    auto startPosition = computeGridLinePosition(startLine, trackSizes, gap);
    auto endPosition = computeGridLinePosition(endLine, trackSizes, gap);
    ASSERT(endPosition >= startPosition);

    return endPosition - startPosition;
}

}
}
}
