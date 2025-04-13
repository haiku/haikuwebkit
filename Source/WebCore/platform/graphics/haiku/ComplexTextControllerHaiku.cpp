/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "ComplexTextController.h"

#include "FontCascade.h"

#include <unicode/ubidi.h>

#include <Point.h>
#include <UnicodeChar.h>

namespace WebCore {

void ComplexTextController::collectComplexTextRunsForCharacters(std::span<const UChar> characters, unsigned stringLocation, const Font* font)
{
    unsigned length = characters.size();

    if (!font) {
        m_complexTextRuns.append(ComplexTextRun::create(m_fontCascade.primaryFont(), characters, stringLocation, 0, length, m_run.ltr()));
        return;
    }

    UErrorCode errorCode = U_ZERO_ERROR;

    UBiDi* text = ubidi_openSized(length, 0, &errorCode);
    ubidi_setPara(text, characters.data(), length, m_run.ltr() ? UBIDI_DEFAULT_LTR : UBIDI_DEFAULT_RTL, NULL, &errorCode);
    int32_t runs = ubidi_countRuns(text, &errorCode);
    if (!U_SUCCESS(errorCode)) {
        ubidi_close(text);
        m_complexTextRuns.append(ComplexTextRun::create(*font, characters, stringLocation, 0, length, m_run.ltr()));
        return;
    }

    for (int32_t run = 0; run < runs; run++) {
        int32_t start, length;
        UBiDiDirection direction = ubidi_getVisualRun(text, run, &start, &length);

        Vector<FloatSize> advances;
        Vector<FloatPoint> origins;
        Vector<Glyph> glyphs;
        Vector<unsigned> stringIndices;

        advances.reserveCapacity(length);
        origins.reserveCapacity(length);
        glyphs.reserveCapacity(length);
        stringIndices.reserveCapacity(length);

        for (unsigned i = start; i < start + length;) {
            stringIndices.append(i);

            UChar32 codepoint;
            U16_NEXT_OR_FFFD(characters, i, start + length, codepoint);
            glyphs.append(codepoint);
        }

        if (direction != UBIDI_LTR) {
            for (auto glyph = glyphs.begin(); glyph != glyphs.end(); ++glyph) {
                if (u_isMirrored(*glyph))
                    *glyph = u_charMirror(*glyph);
            }

            // In theory combining characters should be reversed with their base as a unit,
            // but we don't treat them especially and given their metrics (LTR ones are drawn
            // in the box on their left, and RTL ones on their right) this works for us,
            // except in blocks with overridden direction.
            glyphs.reverse();
            stringIndices.reverse();
        }

        char buffer[256];
        char* tmp = buffer;
        BString utf8;
        for (auto codepoint : glyphs) {
            BUnicodeChar::ToUTF8(codepoint, &tmp);
            if (tmp - buffer > sizeof(buffer) - 4) {
                utf8.Append(buffer, tmp - buffer);
                tmp = buffer;
            }
        }
        if (tmp != buffer)
            utf8.Append(buffer, tmp - buffer);

        unsigned glyphCount = glyphs.size();
        if (glyphCount == 0)
            continue;

        BPoint escapements[glyphCount];
        BPoint offsets[glyphCount];
        BFont bfont = *font->platformData().font();
        float fontSize = bfont.Size();

        bfont.GetEscapements(utf8, glyphCount, NULL, escapements, offsets);
        for (unsigned i = 0; i < glyphCount; i++) {
            advances.append(FloatSize(escapements[i].x * fontSize, escapements[i].y * fontSize));
            origins.append(FloatPoint(offsets[i].x * fontSize, offsets[i].y * fontSize));
        }

        FloatSize initialAdvance = toFloatSize(origins[0]);
        m_complexTextRuns.append(ComplexTextRun::create(advances, origins, glyphs, stringIndices, initialAdvance, *font, characters, stringLocation, start, start + length, direction == UBIDI_LTR));
    }

    ubidi_close(text);

    return;
}

}
