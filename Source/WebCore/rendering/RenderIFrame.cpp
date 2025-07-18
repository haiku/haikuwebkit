/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "RenderIFrame.h"

#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include "RenderWidgetInlines.h"
#include "Settings.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderIFrame);

using namespace HTMLNames;
    
RenderIFrame::RenderIFrame(HTMLIFrameElement& element, RenderStyle&& style)
    : RenderFrameBase(Type::IFrame, element, WTFMove(style))
{
    ASSERT(isRenderIFrame());
}

RenderIFrame::~RenderIFrame() = default;

HTMLIFrameElement& RenderIFrame::iframeElement() const
{
    return downcast<HTMLIFrameElement>(RenderFrameBase::frameOwnerElement());
}

bool RenderIFrame::requiresLayer() const
{
    return RenderFrameBase::requiresLayer() || style().resize() != Resize::None;
}

bool RenderIFrame::isFullScreenIFrame() const
{
    // Some authors implement fullscreen popups as out-of-flow iframes with size set to full viewport (using vw/vh units).
    // The size used may not perfectly match the viewport size so the following heuristic uses a relaxed constraint.
    return style().hasOutOfFlowPosition() && style().usesViewportUnits();
}

void RenderIFrame::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    updateLogicalWidth();
    // No kids to layout as a replaced element.
    updateLogicalHeight();

    clearOverflow();
    addVisualEffectOverflow();
    updateLayerTransform();

    clearNeedsLayout();
}

}
