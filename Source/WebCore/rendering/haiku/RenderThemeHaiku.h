/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
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

#ifndef RenderThemeHaiku_h
#define RenderThemeHaiku_h

#include "RenderTheme.h"
#include "HTMLMediaElement.h"
#include "MediaControlTextTrackContainerElement.h"

namespace WebCore {

class PaintInfo;

class RenderThemeHaiku : public RenderTheme {
private:
    RenderThemeHaiku();
    virtual ~RenderThemeHaiku();

public:
    friend NeverDestroyed<RenderThemeHaiku>;

#if ENABLE(VIDEO)
    Vector<String, 2> mediaControlsScripts() override;
#endif
protected:
    void adjustTextFieldStyle(RenderStyle&, const Element*) const override;
    bool paintTextField(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustTextAreaStyle(RenderStyle&, const Element*) const override;
    bool paintTextArea(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustMenuListStyle(RenderStyle&, const Element*) const override;
    bool paintMenuList(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const override;
    void paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void adjustSliderTrackStyle(RenderStyle&, const Element*) const override;
    bool paintSliderTrack(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustSliderThumbStyle(RenderStyle&, const Element*) const override;

    void adjustSliderThumbSize(RenderStyle&, const Element*) const override;

#if ENABLE(DATALIST_ELEMENT)
    // Returns size of one slider tick mark for a horizontal track.
    // For vertical tracks we rotate it and use it. i.e. Width is always length along the track.
    IntSize sliderTickSize() const override;
    // Returns the distance of slider tick origin from the slider track center.
    int sliderTickOffsetFromTrackCenter() const override;
#endif

    bool paintSliderThumb(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    Color systemColor(CSSValueID, OptionSet<StyleColorOptions>) const override;

    bool paintCheckbox(const RenderElement&, const PaintInfo&, const FloatRect&) override;
    bool paintRadio(const RenderElement&, const PaintInfo&, const FloatRect&) override;
    bool paintButton(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    Style::PreferredSizePair controlSize(StyleAppearance, const FontCascade&, const Style::PreferredSizePair&, float) const override;
private:
    uint32 flagsForObject(const RenderElement&) const;
    rgb_color colorForControl(const RenderObject&) const;
    rgb_color colorForValue(color_which, bool useDarkAppearance) const;

#if ENABLE(VIDEO)
    String mediaControlsBase64StringForIconNameAndType(const String&, const String&) final;
    String mediaControlsFormattedStringForDuration(double) final;

    String m_mediaControlsStyleSheet;
#endif // ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)
};

} // namespace WebCore

#endif // RenderThemeHaiku_h
