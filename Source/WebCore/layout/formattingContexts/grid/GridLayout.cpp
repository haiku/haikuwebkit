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
#include "GridLayout.h"

#include "ImplicitGrid.h"
#include "PlacedGridItem.h"
#include "RenderStyleInlines.h"
#include "LayoutElementBox.h"
#include "UnplacedGridItem.h"
#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {
GridLayout::GridLayout(const GridFormattingContext& gridFormattingContext)
    : m_gridFormattingContext(gridFormattingContext)
{
}

// 8.5. Grid Item Placement Algorithm.
// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
auto GridLayout::placeGridItems(const UnplacedGridItems& unplacedGridItems, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
    const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes)
{
    struct Result {
        PlacedGridItems placedGridItems;
        size_t implicitGridColumnsCount;
        size_t implicitGridRowsCount;
    };

    ImplicitGrid implicitGrid(gridTemplateColumnsTrackSizes.size(), gridTemplateRowsTrackSizes.size());

    // 1. Position anything that’s not auto-positioned.
    auto& nonAutoPositionedGridItems = unplacedGridItems.nonAutoPositionedItems;
    for (auto& nonAutoPositionedItem : nonAutoPositionedGridItems)
        implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem);

    ASSERT(implicitGrid.columnsCount() == gridTemplateColumnsTrackSizes.size() && implicitGrid.rowsCount() == gridTemplateRowsTrackSizes.size(),
        "Since we currently only support placing items which are explicitly placed and fit within the explicit grid, the size of the implicit grid should match the passed in sizes.");

    return Result { implicitGrid.placedGridItems(), implicitGrid.columnsCount(), implicitGrid.rowsCount() };
}

// https://drafts.csswg.org/css-grid-1/#layout-algorithm
void GridLayout::layout(GridFormattingContext::GridLayoutConstraints, const UnplacedGridItems& unplacedGridItems)
{
    CheckedRef gridContainerStyle = this->gridContainerStyle();
    auto& gridTemplateColumnsTrackSizes = gridContainerStyle->gridTemplateColumns().sizes;
    auto& gridTemplateRowsTrackSizes = gridContainerStyle->gridTemplateRows().sizes;

    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    auto [ placedGridItems, implicitGridColumnsCount, implicitGridRowsCount ] = placeGridItems(unplacedGridItems, gridTemplateColumnsTrackSizes, gridTemplateRowsTrackSizes);

    auto columnTrackSizingFunctions = trackSizingFunctions(implicitGridColumnsCount, gridTemplateColumnsTrackSizes);
    auto rowTrackSizingFunctions = trackSizingFunctions(implicitGridRowsCount, gridTemplateRowsTrackSizes);
}

GridLayout::TrackSizingFunctionsList GridLayout::trackSizingFunctions(size_t implicitGridTracksCount, const Vector<Style::GridTrackSize> gridTemplateTrackSizes)
{
    ASSERT(implicitGridTracksCount == gridTemplateTrackSizes.size(), "Currently only support mapping track sizes from explicit grid from grid-template-{columns, rows}");
    UNUSED_VARIABLE(implicitGridTracksCount);

    // https://drafts.csswg.org/css-grid-1/#algo-terms
    return gridTemplateTrackSizes.map([](const Style::GridTrackSize& gridTrackSize) {
        auto minTrackSizingFunction = [&]() {
            // If the track was sized with a minmax() function, this is the first argument to that function.
            if (gridTrackSize.isMinMax())
                return gridTrackSize.minTrackBreadth();

            // If the track was sized with a <flex> value or fit-content() function, auto.
            if (gridTrackSize.isFitContent() || gridTrackSize.minTrackBreadth().isFlex())
                return Style::GridTrackBreadth { CSS::Keyword::Auto { } };

            // Otherwise, the track’s sizing function.
            return gridTrackSize.minTrackBreadth();
        };

        auto maxTrackSizingFunction = [&]() {
            // If the track was sized with a minmax() function, this is the second argument to that function.
            if (gridTrackSize.isMinMax())
                return gridTrackSize.maxTrackBreadth();

            // Otherwise, the track’s sizing function. In all cases, treat auto and fit-content() as max-content,
            // except where specified otherwise for fit-content().
            if (gridTrackSize.maxTrackBreadth().isAuto())
                return Style::GridTrackBreadth { CSS::Keyword::MaxContent { } };

            if (gridTrackSize.isFitContent()) {
                ASSERT_NOT_IMPLEMENTED_YET();
                return Style::GridTrackBreadth { CSS::Keyword::MaxContent { } };
            }

            return gridTrackSize.maxTrackBreadth();
        };

        return TrackSizingFunctions { minTrackSizingFunction(), maxTrackSizingFunction() };
    });
}

const ElementBox& GridLayout::gridContainer() const
{
    return m_gridFormattingContext->root();
}

const RenderStyle& GridLayout::gridContainerStyle() const
{
    return gridContainer().style();
}

} // namespace Layout
} // namespace WebCore
