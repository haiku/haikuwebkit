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

#include "GridFormattingContext.h"
#include "StyleGridTrackBreadth.h"

namespace WebCore {
class RenderStyle;

namespace Style {
struct GridTrackSize;
};

namespace Layout {

class PlacedGridItem;
class UnplacedGridItem;
struct UnplacedGridItems;

class GridLayout {
public:
    GridLayout(const GridFormattingContext&);

    void layout(GridFormattingContext::GridLayoutConstraints, const UnplacedGridItems&);
private:
    struct TrackSizingFunctions {
        Style::GridTrackBreadth min { CSS::Keyword::Auto { } };
        Style::GridTrackBreadth max { CSS::Keyword::Auto { } };
    };
    using PlacedGridItems = Vector<PlacedGridItem>;
    using TrackSizingFunctionsList = Vector<TrackSizingFunctions>;

    static auto placeGridItems(const UnplacedGridItems&, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
        const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes);
    static TrackSizingFunctionsList trackSizingFunctions(size_t implicitGridTracksCount, const Vector<Style::GridTrackSize> gridTemplateTrackSizes);

    const ElementBox& gridContainer() const;
    const RenderStyle& gridContainerStyle() const;

    const CheckedRef<const GridFormattingContext> m_gridFormattingContext;
};

}
}
