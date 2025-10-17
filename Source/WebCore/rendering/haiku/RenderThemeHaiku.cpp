/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 *               2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderThemeHaiku.h"

#include "GraphicsContext.h"
#include "InputTypeNames.h"
#include "NotImplemented.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderElement.h"
#include "RenderStyleSetters.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <ControlLook.h>
#include <View.h>
#include <private/interface/DefaultColors.h>

#include <wtf/text/StringBuilder.h>


namespace WebCore {

static const int sliderThumbWidth = 15;
static const int sliderThumbHeight = 17;

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeHaiku> theme;
    return theme;
}

RenderThemeHaiku::RenderThemeHaiku()
{
}

RenderThemeHaiku::~RenderThemeHaiku()
{
}

bool RenderThemeHaiku::paintSliderTrack(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    rgb_color background = base;
        // TODO: From PaintInfo?
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);
    if (isPressed(object))
    	flags |= BControlLook::B_ACTIVATED;
    if (isDefault(object))
    	flags |= BControlLook::B_DEFAULT_BUTTON;
    be_control_look->DrawSliderBar(view, rect, view->Bounds(), base, background, flags,
        object.style().appearance() == StyleAppearance::SliderHorizontal ?
            B_HORIZONTAL : B_VERTICAL);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(object, info, intRect);
#endif

    return false;
}

void RenderThemeHaiku::adjustSliderTrackStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(CSS::Keyword::None {});
}

void RenderThemeHaiku::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(CSS::Keyword::None {});
}

void RenderThemeHaiku::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    const StyleAppearance& appearance = style.appearance();
    if (appearance == StyleAppearance::SliderVertical) {
        style.setWidth(WebCore::Style::PreferredSize(Length(sliderThumbHeight, LengthType::Fixed)));
        style.setHeight(WebCore::Style::PreferredSize(Length(sliderThumbWidth, LengthType::Fixed)));
    } else if (appearance == StyleAppearance::SliderHorizontal) {
        style.setWidth(WebCore::Style::PreferredSize(Length(sliderThumbWidth, LengthType::Fixed)));
        style.setHeight(WebCore::Style::PreferredSize(Length(sliderThumbHeight, LengthType::Fixed)));
    }
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeHaiku::sliderTickSize() const
{
    return IntSize(1, 6);
}

int RenderThemeHaiku::sliderTickOffsetFromTrackCenter() const
{
    static const int sliderTickOffset = -(sliderThumbHeight / 2 + 1);

    return sliderTickOffset;
}

#endif

bool RenderThemeHaiku::paintSliderThumb(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);
    if (isPressed(object))
    	flags |= BControlLook::B_ACTIVATED;
    if (isDefault(object))
    	flags |= BControlLook::B_DEFAULT_BUTTON;
    be_control_look->DrawSliderThumb(view, rect, view->Bounds(), base, flags,
        object.style().appearance() == StyleAppearance::SliderHorizontal ?
            B_HORIZONTAL : B_VERTICAL);

    return false;
}


#if ENABLE(VIDEO)
Vector<String, 2> RenderThemeHaiku::mediaControlsScripts()
{
#if ENABLE(MODERN_MEDIA_CONTROLS)
    return { StringImpl::createWithoutCopying(std::span<const char>(ModernMediaControlsJavaScript, sizeof(ModernMediaControlsJavaScript))) };
#else
    return { };
#endif
}
#endif

void RenderThemeHaiku::adjustTextFieldStyle(RenderStyle&, const Element*) const
{
}

bool RenderThemeHaiku::paintTextField(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    if (info.context().paintingDisabled())
        return true;

    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    //rgb_color background = base;
        // TODO: From PaintInfo?
    BRect rect(intRect);
    BView* view(info.context().platformContext());
    unsigned flags = flagsForObject(object) & ~BControlLook::B_CLICKED;

    view->PushState();
    be_control_look->DrawTextControlBorder(view, rect, view->Bounds(), base, flags);
    view->PopState();
    return false;
}

void RenderThemeHaiku::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
	adjustTextFieldStyle(style, element);
}

bool RenderThemeHaiku::paintTextArea(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    return paintTextField(object, info, intRect);
}

void RenderThemeHaiku::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
    adjustMenuListButtonStyle(style, element);
}

void RenderThemeHaiku::adjustMenuListButtonStyle(RenderStyle& style, const Element*) const
{
    style.resetBorder();
    style.resetBorderRadius();

    int labelSpacing = be_control_look ? static_cast<int>(be_control_look->DefaultLabelSpacing()) : 3;
    // Position the text correctly within the select box and make the box wide enough to fit the dropdown button
    style.setPaddingTop(WebCore::Style::PaddingEdge(Length(3, LengthType::Fixed)));
    style.setPaddingLeft(WebCore::Style::PaddingEdge(Length(3 + labelSpacing, LengthType::Fixed)));
    style.setPaddingRight(WebCore::Style::PaddingEdge(Length(22, LengthType::Fixed)));
    style.setPaddingBottom(WebCore::Style::PaddingEdge(Length(3, LengthType::Fixed)));

    // Height is locked to auto
    style.setHeight(WebCore::Style::PreferredSize(Length(LengthType::Auto)));

    // Calculate our min-height
    const int menuListButtonMinHeight = 20;
    int minHeight = style.computedFontSize();
    minHeight = std::max(minHeight, menuListButtonMinHeight);

    style.setMinHeight(WebCore::Style::MinimumSize(Length(minHeight, LengthType::Fixed)));
}

void RenderThemeHaiku::paintMenuListButtonDecorations(const RenderBox& object, const PaintInfo& info, const FloatRect& floatRect)
{
    if (!be_control_look)
        return;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.firstChild()->useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect = floatRect;
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(dynamic_cast<RenderElement&>(*object.firstChild())) & ~BControlLook::B_CLICKED;

    view->PushState();
    be_control_look->DrawMenuFieldFrame(view, rect, view->Bounds(), base, base, flags);
    be_control_look->DrawMenuFieldBackground(view, rect, view->Bounds(), base, true, flags);
    view->PopState();
}

bool RenderThemeHaiku::paintCheckbox(const RenderElement& object, const PaintInfo& info, const FloatRect& zoomedRect)
{
    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect(zoomedRect);
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(object) & ~BControlLook::B_CLICKED;

    be_control_look->DrawCheckBox(view, rect, view->Bounds(), base, flags);
    return false;
}

bool RenderThemeHaiku::paintRadio(const RenderElement& object, const PaintInfo& info, const FloatRect& zoomedRect)
{
    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect(zoomedRect);
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(object) & ~BControlLook::B_CLICKED;

    be_control_look->DrawRadioButton(view, rect, view->Bounds(), base, flags);
    return false;
}

bool RenderThemeHaiku::paintButton(const RenderElement& object, const PaintInfo& info, const FloatRect& zoomedRect)
{
    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect(zoomedRect);
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(object);

    be_control_look->DrawButtonFrame(view, rect, view->Bounds(), base, view->ViewColor(), flags);
    be_control_look->DrawButtonBackground(view, rect, view->Bounds(), base, flags);

    return false;
}

Style::PreferredSizePair RenderThemeHaiku::controlSize(StyleAppearance appearance,
    const FontCascade& font, const Style::PreferredSizePair& minimum, float zoom) const
{
    switch (appearance) {
        case StyleAppearance::Checkbox:
        case StyleAppearance::Radio:
        {
            // Keep in sync with min size code in BCheckbox constructor
            float minHeight = (float)ceil(6.0f + font.size());
            return {
                Style::PreferredSize::Fixed { minHeight },
                Style::PreferredSize::Fixed { minHeight }
            };
        }

        default:
            return RenderTheme::controlSize(appearance, font, minimum, zoom);
    }
}

bool RenderThemeHaiku::paintMenuList(const RenderElement&, const PaintInfo&, const FloatRect&)
{
    // This is never called: the list is handled natively as a BMenu.
    return true;
}

uint32 RenderThemeHaiku::flagsForObject(const RenderElement& object) const
{
    uint32 flags = BControlLook::B_BLEND_FRAME;
    if (!isEnabled(object))
        flags |= BControlLook::B_DISABLED;
    if (isFocused(object))
        flags |= BControlLook::B_FOCUSED;
    if (isPressed(object))
        flags |= BControlLook::B_CLICKED;
    if (isChecked(object))
        flags |= BControlLook::B_ACTIVATED;
    if (isHovered(object))
        flags |= BControlLook::B_HOVER;
    return flags;
}


rgb_color RenderThemeHaiku::colorForValue(color_which colorConstant, bool useDarkAppearance) const
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


String RenderThemeHaiku::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    // TODO
    notImplemented();
    return { };
}

String RenderThemeHaiku::mediaControlsFormattedStringForDuration(double durationInSeconds)
{
    // FIXME: Format this somehow, maybe through BDateTime?
    return makeString(durationInSeconds);
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
