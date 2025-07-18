/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#pragma once

#include "ScrollbarThemeComposite.h"

namespace WebCore {

class PlatformMouseEvent;
class Scrollbar;
class ScrollView;

class RenderScrollbarTheme final : public ScrollbarThemeComposite {
public:
    virtual ~RenderScrollbarTheme() = default;
    
    int scrollbarThickness(ScrollbarWidth scrollbarWidth = ScrollbarWidth::Auto, ScrollbarExpansionState expansionState = ScrollbarExpansionState::Expanded, OverlayScrollbarSizeRelevancy overlayRelevancy = OverlayScrollbarSizeRelevancy::IncludeOverlayScrollbarSize) override { return ScrollbarTheme::theme().scrollbarThickness(scrollbarWidth, expansionState, overlayRelevancy); }

    ScrollbarButtonsPlacement buttonsPlacement() const override { return ScrollbarTheme::theme().buttonsPlacement(); }

    bool supportsControlTints() const override { return true; }

    void paintScrollCorner(ScrollableArea&, GraphicsContext&, const IntRect& cornerRect) override;

    ScrollbarButtonPressAction handleMousePressEvent(Scrollbar& scrollbar, const PlatformMouseEvent& event, ScrollbarPart pressedPart) override { return ScrollbarTheme::theme().handleMousePressEvent(scrollbar, event, pressedPart); }

    Seconds initialAutoscrollTimerDelay() override { return ScrollbarTheme::theme().initialAutoscrollTimerDelay(); }
    Seconds autoscrollTimerDelay() override { return ScrollbarTheme::theme().autoscrollTimerDelay(); }

    void registerScrollbar(Scrollbar& scrollbar) override { return ScrollbarTheme::theme().registerScrollbar(scrollbar); }
    void unregisterScrollbar(Scrollbar& scrollbar) override { return ScrollbarTheme::theme().unregisterScrollbar(scrollbar); }

    int minimumThumbLength(Scrollbar&) override;

    void buttonSizesAlongTrackAxis(Scrollbar&, int& beforeSize, int& afterSize);
    
    static RenderScrollbarTheme* renderScrollbarTheme();

private:
    bool hasButtons(Scrollbar&) override;
    bool hasThumb(Scrollbar&) override;

    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    IntRect trackRect(Scrollbar&, bool painting = false) override;

    void willPaintScrollbar(GraphicsContext&, Scrollbar&) override;
    void didPaintScrollbar(GraphicsContext&, Scrollbar&) override;
    
    void paintScrollbarBackground(GraphicsContext&, Scrollbar&) override;
    void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintTrackPiece(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    void paintButton(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintTickmarks(GraphicsContext&, Scrollbar&, const IntRect&) override;

    IntRect constrainTrackRectToTrackPieces(Scrollbar&, const IntRect&) override;
};

} // namespace WebCore
