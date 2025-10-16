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

#pragma once

#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

class PlacedGridItem;
class UnplacedGridItem;

using PlacedGridItems = Vector<PlacedGridItem>;

// https://drafts.csswg.org/css-grid-1/#implicit-grids
class ImplicitGrid {
public:
    ImplicitGrid(size_t explicitColumnsCount, size_t explicitRowsCount);

    size_t rowsCount() const { return m_gridMatrix.size(); }
    size_t columnsCount() const { return rowsCount() ? m_gridMatrix[0].size() : 0; }

    void insertUnplacedGridItem(const UnplacedGridItem&);

    PlacedGridItems placedGridItems() const;

private:
    using GridMatrix = Vector<Vector<std::optional<UnplacedGridItem>>>;
    GridMatrix m_gridMatrix;
};

} // namespace Layout
} // namespace WebCore
