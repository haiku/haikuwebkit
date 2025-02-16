/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "FontCascade.h"
#include "FontDescription.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "TextEncoding.h"
#include "NotImplemented.h"
#include <wtf/text/CString.h>
#include <Font.h>
#include <String.h>
#include <UnicodeChar.h>
#include <View.h>


namespace WebCore {

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return false;
}

void FontCascade::drawGlyphs(GraphicsContext& graphicsContext, const Font& font,
    std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances,
    const FloatPoint& point, WebCore::FontSmoothingMode smoothing)
{
    BView* view = graphicsContext.platformContext();
    view->PushState();

    rgb_color color = graphicsContext.fillColor();

    if (color.alpha < 255 || graphicsContext.isInTransparencyLayer())
        view->SetDrawingMode(B_OP_ALPHA);
    else
        view->SetDrawingMode(B_OP_OVER);
    view->SetHighColor(color);
    BFont bfont;
    // Sometimes we will end up here with a reference to a NULL font… oh well.
    if (&font == NULL)
        bfont = be_plain_font;
    else
        bfont = *font.platformData().font();

    if (smoothing == FontSmoothingMode::NoSmoothing)
        bfont.SetFlags(B_DISABLE_ANTIALIASING);
    else
        bfont.SetFlags(B_FORCE_ANTIALIASING);
    view->SetFont(&bfont);

    BPoint offsets[glyphs.size()];
    char buffer[4];
    BString utf8;
    int32 realGlyphCount = 0;
    float offset = point.x();
    for (unsigned i = 0; i < glyphs.size(); i++) {
        Glyph glyph = glyphs[i];
        if (glyph == 0) {
            if (advances[i].width() == 0.0) {
                // These are fake glyphs to keep GlyphBuffer vectors in sync with text runs
                // when a surrogate pair is found (cf. addToGlyphBuffer in WidthIterator.cpp).
                continue;
            }
            glyph = 0xfdd1;
        }

        offsets[realGlyphCount].x = offset;
        offsets[realGlyphCount].y = point.y();
        offset += advances[i].width();

        char* tmp = buffer;
        BUnicodeChar::ToUTF8(glyph, &tmp);
        utf8.Append(buffer, tmp - buffer);
        realGlyphCount++;
    }

    view->DrawString(utf8, offsets, realGlyphCount);
    view->PopState();
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return false;
}

Path Font::platformPathForGlyph(Glyph glyph) const
{
	notImplemented();
	UNUSED_PARAM(glyph);
	
	return Path();
}

bool FontCascade::canUseGlyphDisplayList(const RenderStyle&)
{
    return true;
}

ResolvedEmojiPolicy FontCascade::resolveEmojiPolicy(FontVariantEmoji fontVariantEmoji, char32_t)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=259205 We can't return RequireText or RequireEmoji
    // unless we have a way of knowing whether a font/glyph is color or not.
    switch (fontVariantEmoji) {
    case FontVariantEmoji::Normal:
    case FontVariantEmoji::Unicode:
        return ResolvedEmojiPolicy::NoPreference;
    case FontVariantEmoji::Text:
        return ResolvedEmojiPolicy::RequireText;
    case FontVariantEmoji::Emoji:
        return ResolvedEmojiPolicy::RequireEmoji;
    }
    return ResolvedEmojiPolicy::NoPreference;
}

} // namespace WebCore

