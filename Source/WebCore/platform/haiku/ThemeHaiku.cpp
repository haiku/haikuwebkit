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
#include "ControlStates.h"
#include "GraphicsContext.h"
#include "LengthSize.h"

#include <ControlLook.h>

#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const double focusRingOpacity = 0.8; // Keep in sync with focusRingOpacity in RenderThemeHaiku.
static const unsigned focusLineWidth = 2;
static const unsigned arrowSize = 16;
static const int buttonBorderSize = 1; // Keep in sync with menuListButtonBorderSize in RenderThemeHaiku.


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
	rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR);
		
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

LengthSize ThemeHaiku::controlSize(StyleAppearance appearance, const FontCascade& fontCascade, const LengthSize& zoomedSize, float zoomFactor) const
{
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return Theme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);

    switch (appearance) {
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
        LengthSize buttonSize = zoomedSize;
        if (buttonSize.width.isIntrinsicOrAuto())
            buttonSize.width = Length(16 * zoomFactor, LengthType::Fixed);
        if (buttonSize.height.isIntrinsicOrAuto())
            buttonSize.height = Length(16 * zoomFactor, LengthType::Fixed);
        return buttonSize;
    }
    case StyleAppearance::InnerSpinButton: {
        LengthSize spinButtonSize = zoomedSize;
        if (spinButtonSize.width.isIntrinsicOrAuto())
            spinButtonSize.width = Length(static_cast<int>(arrowSize * zoomFactor), LengthType::Fixed);
        if (spinButtonSize.height.isIntrinsicOrAuto() || fontCascade.size() > arrowSize)
            spinButtonSize.height = Length(fontCascade.size(), LengthType::Fixed);
        return spinButtonSize;
    }
    default:
        break;
    }

    return Theme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);
}

LengthSize ThemeHaiku::minimumControlSize(StyleAppearance, const FontCascade&, const LengthSize& zoomedSize, float) const
{
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return zoomedSize;

    LengthSize minSize = zoomedSize;
    if (minSize.width.isIntrinsicOrAuto())
        minSize.width = Length(0, LengthType::Fixed);
    if (minSize.height.isIntrinsicOrAuto())
        minSize.height = Length(0, LengthType::Fixed);
    return minSize;
}

void ThemeHaiku::paint(StyleAppearance appearance, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float, ScrollView*, float, float, bool, bool useDarkAppearance, const Color& effectiveAccentColor)
{
    switch (appearance) {
    case StyleAppearance::Checkbox:
        paintCheckbox(states, context, zoomedRect, useDarkAppearance, effectiveAccentColor);
        break;
    case StyleAppearance::Radio:
        paintRadio(states, context, zoomedRect, useDarkAppearance, effectiveAccentColor);
        break;
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::SquareButton:
        paintButton(states, context, zoomedRect, useDarkAppearance);
        break;
    case StyleAppearance::InnerSpinButton:
        paintSpinButton(states, context, zoomedRect, useDarkAppearance);
        break;
    default:
        break;
    }
}

void ThemeHaiku::paintCheckbox(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance, const Color& effectiveAccentColor)
{
	uint32 flags = 0;
	if (!states.states().contains(ControlStates::States::Enabled))
		flags |= BControlLook::B_DISABLED;
	if (states.states().contains(ControlStates::States::Focused))
		flags |= BControlLook::B_FOCUSED;
	if (states.states().contains(ControlStates::States::Checked))
		flags |= BControlLook::B_ACTIVATED;
	if (states.states().contains(ControlStates::States::Indeterminate))
		flags |= BControlLook::B_PARTIALLY_ACTIVATED;
	if (states.states().contains(ControlStates::States::Hovered))
		flags |= BControlLook::B_HOVER;

	BRect rect(zoomedRect);
	be_control_look->DrawCheckBox(graphicsContext.platformContext(), rect, graphicsContext.platformContext()->Bounds(), colorForValue(B_CONTROL_BACKGROUND_COLOR), flags);
}

void ThemeHaiku::paintRadio(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance, const Color& effectiveAccentColor)
{
	uint32 flags = 0;
	if (!states.states().contains(ControlStates::States::Enabled))
		flags |= BControlLook::B_DISABLED;
	if (states.states().contains(ControlStates::States::Focused))
		flags |= BControlLook::B_FOCUSED;
	if (states.states().contains(ControlStates::States::Checked))
		flags |= BControlLook::B_ACTIVATED;
	if (states.states().contains(ControlStates::States::Indeterminate))
		flags |= BControlLook::B_PARTIALLY_ACTIVATED;
	if (states.states().contains(ControlStates::States::Hovered))
		flags |= BControlLook::B_HOVER;

	BRect rect(zoomedRect);
	be_control_look->DrawRadioButton(graphicsContext.platformContext(), rect, graphicsContext.platformContext()->Bounds(), colorForValue(B_CONTROL_BACKGROUND_COLOR), flags);
}

void ThemeHaiku::paintButton(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance)
{
	rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR);
	
	
	if (states.states().contains(ControlStates::States::Pressed))
		flags |= BControlLook::B_ACTIVATED;
	if (states.states().contains(ControlStates::States::Default))
		flags |= BControlLook::B_DEFAULT_BUTTON;
	if (!states.states().contains(ControlStates::States::Enabled))
		flags |= BControlLook::B_DISABLED;
	if (states.states().contains(ControlStates::States::Focused))
		flags |= BControlLook::B_FOCUSED;
	
	
	BRect r(zoomedRect);
	be_control_look->DrawButtonFrame(graphicsContext.platformContext(), rect, graphicsContext.platformContext()->Bounds(), base, base, flags);
	be_control_look->DrawButtonBackground(graphicsContext.platformContext(), rect, graphicsContext.platformContext()->Bounds(), base, flags);
}

void ThemeHaiku::paintSpinButton(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance)
{
	// There is no way to ask the controlLook to draw this.
	// This is adapted from haiku/src/kits/interface/AbstractSpinner.cpp:340
	// Git revision 3c3462995f23575ca9476b285de758205de45b94
	
	BRect rect(zoomedRect);

	bool isEnabled = states.states().contains(ControlStates::States::Enabled)
	float frameTint = isEnabled ? B_DARKEN_1_TINT : B_NO_TINT ;

	float fgTint;
	if (!isEnabled)
		fgTint = B_DARKEN_1_TINT;
	else if (fIsMouseDown)
		fgTint = B_DARKEN_MAX_TINT;
	else
		fgTint = 1.777f;	// 216 --> 48.2 (48)

	float bgTint;
	if (isEnabled && (states.states().contains(ControlStates::States::Hovered))
		bgTint = B_DARKEN_1_TINT;
	else
		bgTint = B_NO_TINT;

	// Original uses B_PANEL_BACKGROUND_COLOR
	rgb_color bgColor = ui_color(B_CONTROL_BACKGROUND_COLOR);
	if (bgColor.red + bgColor.green + bgColor.blue <= 128 * 3) {
		// if dark background make the tint lighter
		frameTint = 2.0f - frameTint;
		fgTint = 2.0f - fgTint;
		bgTint = 2.0f - bgTint;
	}

	uint32 borders = be_control_look->B_TOP_BORDER
		| be_control_look->B_BOTTOM_BORDER;

	if (states.states().contains(ControlStates::States::SpinUp))
		borders |= be_control_look->B_RIGHT_BORDER;
	else
		borders |= be_control_look->B_LEFT_BORDER;

	uint32 flags = states.states().contains(ControlStates::States::Pressed) ? BControlLook::B_ACTIVATED : 0;
	flags |= !isEnabled ? BControlLook::B_DISABLED : 0;

	// draw the button
	be_control_look->DrawButtonFrame(this, rect, graphicsContext.platformContext()->Bounds(),
		tint_color(bgColor, frameTint), bgColor, flags, borders);
	be_control_look->DrawButtonBackground(this, rect, graphicsContext.platformContext()->Bounds(),
		tint_color(bgColor, bgTint), flags, borders);

	// This is the default case of the original implementation
	// The original file also contained arrows
	BFont font;
	fParent->GetFont(&font);
	float inset = floorf(font.Size() / 4);
	rect.InsetBy(inset, inset);

	if (rect.IntegerWidth() % 2 != 0)
		rect.right -= 1;

	if (rect.IntegerHeight() % 2 != 0)
		rect.bottom -= 1;

	SetHighColor(tint_color(bgColor, fgTint));

	// draw the +/-
	float halfHeight = floorf(rect.Height() / 2);
	StrokeLine(BPoint(rect.left, rect.top + halfHeight),
		BPoint(rect.right, rect.top + halfHeight));
	if (states.states().contains(ControlStates::States::SpinUp)) {
		float halfWidth = floorf(rect.Width() / 2);
		StrokeLine(BPoint(rect.left + halfWidth, rect.top + 1),
			BPoint(rect.left + halfWidth, rect.bottom - 1));
	}
}

rgb_color ThemeHaiku::colorForValue(color_which colorConstant, bool useDarkAppearance) const
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

Color RenderThemeHaiku::systemColor(CSSValueID cssValueID, OptionSet<StyleColorOptions> options) const
{
	const bool useDarkAppearance = options.contains(StyleColorOptions::UseDarkAppearance);

	switch (cssValueID) {
	case CSSValueButtonface:
		return colorForValue(B_CONTROL_BACKGROUND_COLOR, useDarkAppearance);
		
		// Doesn't exist?
		//case CSSValueButtonborder:
		//    return colorForValue(B_CONTROL_BORDER_COLOR, useDarkAppearence);
		
	case CSSValueActivebuttontext:
	case CSSValueButtontext:
		return colorForValue(B_CONTROL_TEXT_COLOR, useDarkAppearance);
		
	case CSSValueField:
	case CSSValueCanvas:
	case CSSValueWindow:
		return colorForValue(B_DOCUMENT_BACKGROUND_COLOR, useDarkAppearance);
		
	case CSSValueCanvastext:
	case CSSValueFieldtext:
		return colorForValue(B_DOCUMENT_TEXT_COLOR, useDarkAppearance);
		
	case CSSValueWebkitFocusRingColor:
	case CSSValueActiveborder:
	case CSSValueHighlight:
		return colorForValue(B_CONTROL_HIGHLIGHT_COLOR, useDarkAppearance);
		
	case CSSValueHighlighttext:
		return colorForValue(B_CONTROL_TEXT_COLOR, useDarkAppearance);
		
	case CSSValueWebkitLink:
	case CSSValueLinktext:
		return colorForValue(B_LINK_TEXT_COLOR, useDarkAppearance);
		
	case CSSValueVisitedtext:
		return colorForValue(B_LINK_VISITED_COLOR, useDarkAppearance);
		
		// case CSSValueWebkitActivetext:
	case CSSValueWebkitActivelink:
		return colorForValue(B_LINK_ACTIVE_COLOR, useDarkAppearance);
		
	/* is there any haiku colors that make sense to use here?
	 case CSSValueSelecteditem:
	 case CSSValueSelecteditemtext:
	 case CSSValueMark:
	 case CSSValueMarkText:
	 */
	default:
		return RenderTheme::systemColor(cssValueID, options);
	}
}
} // namespace WebCore
