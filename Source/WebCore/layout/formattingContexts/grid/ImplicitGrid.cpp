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
#include "ImplicitGrid.h"

#include "PlacedGridItem.h"
#include "UnplacedGridItem.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

// The implicit grid is created from the explicit grid + items that are placed outside
// of the explicit grid. Since we know the explicit tracks from style we start the
// implicit grid as exactly the explicit grid and allow placement to add implicit
// tracks and grow the grid.
ImplicitGrid::ImplicitGrid(size_t gridTemplateColumnsCount, size_t gridTemplateRowsCount)
    : m_gridMatrix(Vector(gridTemplateRowsCount, Vector<std::optional<UnplacedGridItem>>(gridTemplateColumnsCount)))
{
}

void ImplicitGrid::insertUnplacedGridItem(const UnplacedGridItem& unplacedGridItem)
{
    // https://drafts.csswg.org/css-grid/#common-uses-numeric
    auto explicitColumnStart = unplacedGridItem.explicitColumnStart();
    auto explicitColumnEnd = unplacedGridItem.explicitColumnEnd();
    if (explicitColumnStart < 0 || explicitColumnEnd < 0) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitColumnEnd <= explicitColumnStart) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto columnsCount = static_cast<int>(this->columnsCount());
    if (explicitColumnStart > columnsCount || explicitColumnEnd > columnsCount) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto explicitRowStart = unplacedGridItem.explicitRowStart();
    auto explicitRowEnd = unplacedGridItem.explicitRowEnd();
    if (explicitRowStart < 0 || explicitRowEnd < 0) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitRowEnd <= explicitRowStart) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto rowsCount = static_cast<int>(this->rowsCount());
    if (explicitRowStart > rowsCount || explicitRowEnd > rowsCount) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitColumnEnd - explicitColumnStart > 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitRowEnd - explicitRowStart > 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto columnsRange = WTF::Range(explicitColumnStart, explicitColumnEnd);
    auto rowsRange = WTF::Range(explicitRowStart, explicitRowEnd);
    for (auto rowIndex = rowsRange.begin(); rowIndex < rowsRange.end(); ++rowIndex) {
        for (auto columnIndex = columnsRange.begin(); columnIndex < columnsRange.end(); ++columnIndex)
            m_gridMatrix[rowIndex][columnIndex] = unplacedGridItem;
    }

}

PlacedGridItems ImplicitGrid::placedGridItems() const
{
    HashSet<UnplacedGridItem> processedUnplacedGridItems;
    PlacedGridItems placedGridItems;

    for (size_t rowIndex = 0; rowIndex < m_gridMatrix.size(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < m_gridMatrix[rowIndex].size(); ++columnIndex) {

            auto unplacedGridItem = m_gridMatrix[rowIndex][columnIndex];
            if (!unplacedGridItem || processedUnplacedGridItems.contains(*unplacedGridItem))
                continue;

            processedUnplacedGridItems.add(*unplacedGridItem);
            placedGridItems.append({ *unplacedGridItem, { columnIndex, columnIndex + 1, rowIndex, rowIndex + 1 } });
        }
    }
    return placedGridItems;
}

} // namespace Layout
} // namespace WebCore
