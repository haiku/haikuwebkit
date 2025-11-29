/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayoutIntegrationFormattingContextLayout.h"

#include "BlockLayoutState.h"
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderObjectInlines.h"

namespace WebCore {
namespace LayoutIntegration {

static inline const Layout::ElementBox& rootLayoutBox(const Layout::ElementBox& child)
{
    auto* ancestor = &child.parent();
    while (!ancestor->isInitialContainingBlock()) {
        if (ancestor->establishesFormattingContext())
            break;
        ancestor = &ancestor->parent();
    }
    return *ancestor;
}

void layoutWithFormattingContextForBox(const Layout::ElementBox& box, std::optional<LayoutUnit> widthConstraint, std::optional<LayoutUnit> heightConstraint, Layout::LayoutState& layoutState)
{
    auto& renderer = downcast<RenderBox>(*box.rendererForIntegration());

    if (widthConstraint) {
        renderer.setOverridingBorderBoxLogicalWidth(*widthConstraint);
        renderer.setNeedsLayout(MarkOnlyThis);
    }

    if (heightConstraint) {
        renderer.setOverridingBorderBoxLogicalHeight(*heightConstraint);
        renderer.setNeedsLayout(MarkOnlyThis);
    }

    renderer.layoutIfNeeded();

    if (widthConstraint)
        renderer.clearOverridingBorderBoxLogicalWidth();

    auto updater = BoxGeometryUpdater { layoutState, rootLayoutBox(box) };
    updater.updateBoxGeometryAfterIntegrationLayout(box, widthConstraint.value_or(renderer.containingBlock()->contentBoxLogicalWidth()));
}

void layoutWithFormattingContextForBlockInInline(const Layout::ElementBox& block, LayoutPoint blockLogicalTopLeft, Layout::BlockLayoutState& parentBlockLayoutState, Layout::LayoutState& layoutState)
{
    auto& placedFloats = parentBlockLayoutState.placedFloats();

    auto populateRootRendererWithFloatsFromIFC = [&] {
        auto& rootBlockContainer = downcast<RenderBlockFlow>(*rootLayoutBox(block).rendererForIntegration());
        for (auto& floatItem : placedFloats.list()) {
            auto* layoutBox = floatItem.layoutBox();
            if (!layoutBox) {
                // Floats inherited by IFC do not have associated layout boxes.
                continue;
            }
            auto& floatingObject = rootBlockContainer.insertFloatingBox(downcast<RenderBox>(*layoutBox->rendererForIntegration()));
            if (floatingObject.isPlaced()) {
                // We have already inserted this float when laying out a previous middle-block.
                continue;
            }

            floatingObject.setFrameRect(Layout::BoxGeometry::marginBoxRect(floatItem.boxGeometry()));
            floatingObject.setIsPlaced(true);
        }
    };
    populateRootRendererWithFloatsFromIFC();

    // FIXME: We need to run this through the render tree code of "estimateLogicalTopPosition"
    downcast<RenderBox>(*block.rendererForIntegration()).setLogicalTop(blockLogicalTopLeft.y());

    layoutWithFormattingContextForBox(block, { }, { }, layoutState);
    ASSERT(!block.rendererForIntegration()->needsLayout());

    auto populateIFCWithNewlyPlacedFloats = [&] {
        auto* renderBlockFlow = dynamicDowncast<RenderBlockFlow>(*block.rendererForIntegration());
        if (!renderBlockFlow)
            return;

        auto& blockBoxGeometry = layoutState.ensureGeometryForBox(block);
        blockBoxGeometry.setTopLeft(LayoutPoint { blockBoxGeometry.marginStart(), blockBoxGeometry.marginBefore() });

        if (!renderBlockFlow->containsFloats() || renderBlockFlow->createsNewFormattingContext())
            return;

        for (auto& floatingObject : *renderBlockFlow->floatingObjectSet()) {
            if (!floatingObject->isDescendant())
                continue;

            auto floatRect = floatingObject->frameRect();

            auto boxGeometry = Layout::BoxGeometry { };
            boxGeometry.setTopLeft(blockLogicalTopLeft + floatRect.location());
            boxGeometry.setContentBoxWidth(floatRect.width());
            boxGeometry.setContentBoxHeight(floatRect.height());
            boxGeometry.setBorder({ });
            boxGeometry.setPadding({ });
            boxGeometry.setHorizontalMargin({ });
            boxGeometry.setVerticalMargin({ });

            auto shapeOutsideInfo = floatingObject->renderer().shapeOutsideInfo();
            RefPtr shape = shapeOutsideInfo ? &shapeOutsideInfo->computedShape() : nullptr;

            auto usedPosition = RenderStyle::usedFloat(floatingObject->renderer()) == UsedFloat::Left ? Layout::PlacedFloats::Item::Position::Start : Layout::PlacedFloats::Item::Position::End;
            placedFloats.add({ usedPosition, boxGeometry, floatRect.location(), WTFMove(shape) });
        }
    };
    populateIFCWithNewlyPlacedFloats();
}

LayoutUnit formattingContextRootLogicalWidthForType(const Layout::ElementBox& box, LogicalWidthType logicalWidthType)
{
    ASSERT(box.establishesFormattingContext());

    auto& renderer = downcast<RenderBox>(*box.rendererForIntegration());
    switch (logicalWidthType) {
    case LogicalWidthType::PreferredMaximum:
        return renderer.maxPreferredLogicalWidth();
    case LogicalWidthType::PreferredMinimum:
        return renderer.minPreferredLogicalWidth();
    case LogicalWidthType::MaxContent:
    case LogicalWidthType::MinContent: {
        auto minimunLogicalWidth = LayoutUnit { };
        auto maximumLogicalWidth = LayoutUnit { };
        renderer.computeIntrinsicLogicalWidths(minimunLogicalWidth, maximumLogicalWidth);
        return logicalWidthType == LogicalWidthType::MaxContent ? maximumLogicalWidth : minimunLogicalWidth;
    }
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

LayoutUnit formattingContextRootLogicalHeightForType(const Layout::ElementBox& box, LogicalHeightType logicalHeightType)
{
    ASSERT(box.establishesFormattingContext());

    auto& renderer = downcast<RenderBox>(*box.rendererForIntegration());
    switch (logicalHeightType) {
    case LogicalHeightType::MinContent: {
        // Since currently we can't ask RenderBox for content height, this is limited to flex items
        // where the legacy flex layout "fixed" this by caching the content height in RenderBox::updateLogicalHeight
        // before additional height constraints applied.
        if (auto* flexContainer = dynamicDowncast<RenderFlexibleBox>(renderer.parent()))
            return flexContainer->cachedFlexItemIntrinsicContentLogicalHeight(renderer);
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

}
}
