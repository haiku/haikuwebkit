/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
#include "ThemeHaiku.h"

#include "Color.h"
#include "ColorBlending.h"
#include "GraphicsContext.h"

#include <ControlLook.h>
#include <private/interface/DefaultColors.h>

#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const double focusRingOpacity = 0.8; // Keep in sync with focusRingOpacity in RenderThemeHaiku.
static const unsigned focusLineWidth = 2;
static const unsigned arrowSize = 16;


Theme& Theme::singleton()
{
    static NeverDestroyed<ThemeHaiku> theme;
    return theme;
}

Color ThemeHaiku::focusColor(const Color& accentColor)
{
    return accentColor.colorWithAlphaMultipliedBy(focusRingOpacity);
}

static inline float getRectRadius(const FloatRect& rect, int offset)
{
    return (std::min(rect.width(), rect.height()) + offset) / 2;
}

void ThemeHaiku::paintFocus(GraphicsContext& graphicsContext, const FloatRect& rect, int offset, const Color& color, PaintRounded rounded)
{
    FloatRect focusRect = rect;
    focusRect.inflate(offset);

    float radius = (rounded == PaintRounded::Yes) ? getRectRadius(rect, offset) : 2;

    Path path;
    path.addRoundedRect(focusRect, { radius, radius });
    paintFocus(graphicsContext, path, color);
}

void ThemeHaiku::paintFocus(GraphicsContext& graphicsContext, const Path& path, const Color& color)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.beginTransparencyLayer(color.alphaAsFloat());
    // Since we cut off a half of it by erasing the rect contents, and half
    // of the stroke ends up inside that area, it needs to be twice as thick.
    graphicsContext.setStrokeThickness(focusLineWidth * 2);
    graphicsContext.setLineCap(LineCap::Round);
    graphicsContext.setLineJoin(LineJoin::Round);
    graphicsContext.setStrokeColor(color.opaqueColor());
    graphicsContext.strokePath(path);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setCompositeOperation(CompositeOperator::Clear);
    graphicsContext.fillPath(path);
    graphicsContext.setCompositeOperation(CompositeOperator::SourceOver);
    graphicsContext.endTransparencyLayer();
}

void ThemeHaiku::paintFocus(GraphicsContext& graphicsContext, const Vector<FloatRect>& rects, const Color& color, PaintRounded rounded)
{
    Path path;
    for (const auto& rect : rects) {
        float radius = (rounded == PaintRounded::Yes) ? getRectRadius(rect, 0) : 2;

        path.addRoundedRect(rect, { radius, radius });
    }
    paintFocus(graphicsContext, path, color);
}

void ThemeHaiku::paintArrow(GraphicsContext& graphicsContext, const FloatRect& rect, ArrowDirection direction, bool useDarkAppearance)
{
	rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, useDarkAppearance);
		
	BRect r(rect);

	switch (direction) {
	case ArrowDirection::Down:
		be_control_look-> DrawArrowShape(graphicsContext.platformContext(), r, graphicsContext.platformContext()->Bounds(), base, 1);
		break;
	case ArrowDirection::Up:
		be_control_look-> DrawArrowShape(graphicsContext.platformContext(), r, graphicsContext.platformContext()->Bounds(), base, 0);
			break;
	}
}

rgb_color ThemeHaiku::colorForValue(color_which colorConstant, bool useDarkAppearance)
{
		rgb_color systemColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
		if (useDarkAppearance) {
				if (systemColor.Brightness() > 127) // system is in light mode, but we need a dark color
						return BPrivate::GetSystemColor(colorConstant, true);
		} else {
				if (systemColor.Brightness() < 127) // system is in dark mode but we need a light color
						return BPrivate::GetSystemColor(colorConstant, false);
		}
		return ui_color(colorConstant);
}

} // namespace WebCore
