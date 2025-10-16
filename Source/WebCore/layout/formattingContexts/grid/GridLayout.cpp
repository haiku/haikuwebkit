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

// https://drafts.csswg.org/css-grid-1/#layout-algorithm
void GridLayout::layout(GridFormattingContext::GridLayoutConstraints, const UnplacedGridItems& unplacedGridItems)
{
    CheckedRef gridContainerStyle = this->gridContainerStyle();
    auto& gridTemplateColumnsTrackSizes = gridContainerStyle->gridTemplateColumns().sizes;
    auto& gridTemplateRowsTrackSizes = gridContainerStyle->gridTemplateRows().sizes;

    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    auto placedGridItems = placeGridItems(unplacedGridItems, gridTemplateColumnsTrackSizes.size(), gridTemplateRowsTrackSizes.size());
    UNUSED_VARIABLE(placedGridItems);
}

// 8.5. Grid Item Placement Algorithm.
// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
GridLayout::PlacedGridItems GridLayout::placeGridItems(const UnplacedGridItems& unplacedGridItems, size_t gridTemplateColumnsTracksCount, size_t gridTemplateRowsTracksCount)
{
    ImplicitGrid implicitGrid(gridTemplateColumnsTracksCount, gridTemplateRowsTracksCount);

    // 1. Position anything that’s not auto-positioned.
    auto& nonAutoPositionedGridItems = unplacedGridItems.nonAutoPositionedItems;
    for (auto& nonAutoPositionedItem : nonAutoPositionedGridItems)
        implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem);
    ASSERT(implicitGrid.columnsCount() == gridTemplateColumnsTracksCount && implicitGrid.rowsCount()== gridTemplateRowsTracksCount,
        "Since we currently only support placing items which are explicitly placed and fit within the explicit grid, the size of the implicit grid should match the passed in sizes.");
    return implicitGrid.placedGridItems();
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
