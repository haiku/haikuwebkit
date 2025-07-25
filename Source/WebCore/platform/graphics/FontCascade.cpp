/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FontCascade.h"

#include "ComplexTextController.h"
#include "DisplayList.h"
#include "DisplayListRecorderImpl.h"
#include "FloatRect.h"
#include "FontCache.h"
#include "FontCascadeInlines.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "TextRun.h"
#include "WidthIterator.h"
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FontCascade);

using namespace WTF::Unicode;

FontCascade::CodePath FontCascade::s_codePath = CodePath::Auto;

static std::atomic<unsigned> lastFontCascadeGeneration { 0 };

// ============================================================================================
// FontCascade Implementation (Cross-Platform Portion)
// ============================================================================================

FontCascade::FontCascade()
{
}

FontCascade::FontCascade(FontCascadeDescription&& description)
    : m_fontDescription(WTFMove(description))
    , m_generation(++lastFontCascadeGeneration)
    , m_useBackslashAsYenSymbol(computeUseBackslashAsYenSymbol())
    , m_enableKerning(computeEnableKerning())
    , m_requiresShaping(computeRequiresShaping())
{
    m_fontDescription.setShouldDisableLigaturesForSpacing(false);
}

FontCascade::FontCascade(FontCascadeDescription&& description, const FontCascade& other)
    : m_fontDescription(WTFMove(description))
    , m_spacing(other.m_spacing)
    , m_generation(++lastFontCascadeGeneration)
    , m_useBackslashAsYenSymbol(computeUseBackslashAsYenSymbol())
    , m_enableKerning(computeEnableKerning())
    , m_requiresShaping(computeRequiresShaping())
{
}

FontCascade::FontCascade(const FontCascade& other)
    : CanMakeWeakPtr<FontCascade>()
    , CanMakeCheckedPtr<FontCascade>()
    , m_fontDescription(other.m_fontDescription)
    , m_spacing(other.m_spacing)
    , m_fonts(other.m_fonts)
    , m_fontSelector(other.m_fontSelector)
    , m_generation(other.m_generation)
    , m_useBackslashAsYenSymbol(other.m_useBackslashAsYenSymbol)
    , m_enableKerning(computeEnableKerning())
    , m_requiresShaping(computeRequiresShaping())
{
}

FontCascade& FontCascade::operator=(const FontCascade& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fonts = other.m_fonts;
    m_spacing = other.m_spacing;
    m_generation = other.m_generation;
    m_useBackslashAsYenSymbol = other.m_useBackslashAsYenSymbol;
    m_enableKerning = other.m_enableKerning;
    m_requiresShaping = other.m_requiresShaping;
    m_fontSelector = other.m_fontSelector;
    return *this;
}

bool FontCascade::operator==(const FontCascade& other) const
{
    if (m_fontDescription != other.m_fontDescription || m_spacing != other.m_spacing)
        return false;

    if (m_fonts != other.m_fonts)
        return false;

    if (!m_fonts || !other.m_fonts)
        return false;

    if (fontSelector() != other.fontSelector())
        return false;

    // Can these cases actually somehow occur? All fonts should get wiped out by full style recalc.
    if (fontSelectorVersion() != other.fontSelectorVersion())
        return false;

    if (m_fonts->generation() != other.m_fonts->generation())
        return false;

    return true;
}

bool FontCascade::isCurrent(const FontSelector& fontSelector) const
{
    if (!m_fonts)
        return false;
    if (m_fonts->generation() != FontCache::forCurrentThread()->generation())
        return false;
    if (fontSelectorVersion() != fontSelector.version())
        return false;

    return true;
}

unsigned FontCascade::fontSelectorVersion() const
{
    return m_fontSelector ? Ref { *m_fontSelector }->version() : 0;
}

void FontCascade::updateFonts(Ref<FontCascadeFonts>&& fonts) const
{
    // FIXME: Ideally we'd only update m_generation if the fonts changed.
    m_fonts = WTFMove(fonts);
    m_generation = ++lastFontCascadeGeneration;
}

void FontCascade::update(RefPtr<FontSelector>&& fontSelector) const
{
    m_fontSelector = WTFMove(fontSelector);
    FontCache::forCurrentThread()->updateFontCascade(*this);
}

GlyphBuffer FontCascade::layoutText(CodePath codePathToUse, const TextRun& run, unsigned from, unsigned to, ForTextEmphasisOrNot forTextEmphasis) const
{
    if (codePathToUse != CodePath::Complex)
        return layoutSimpleText(run, from, to, forTextEmphasis);

    return layoutComplexText(run, from, to, forTextEmphasis);
}

float FontCascade::letterSpacing() const
{
    switch (m_spacing.letter.type()) {
    case LengthType::Fixed:
        return m_spacing.letter.value();
    case LengthType::Percent:
        return m_spacing.letter.percent() / 100 * size();
    case LengthType::Calculated:
        return m_spacing.letter.nonNanCalculatedValue(size());
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

float FontCascade::wordSpacing() const
{
    switch (m_spacing.word.type()) {
    case LengthType::Fixed:
        return m_spacing.word.value();
    case LengthType::Percent:
        return m_spacing.word.percent() / 100 * size();
    case LengthType::Calculated:
        return m_spacing.word.nonNanCalculatedValue(size());
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

FloatSize FontCascade::drawText(GraphicsContext& context, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to, CustomFontNotReadyAction customFontNotReadyAction) const
{
    unsigned destination = to.value_or(run.length());
    auto glyphBuffer = layoutText(codePath(run, from, to), run, from, destination);
    glyphBuffer.flatten();

    if (glyphBuffer.isEmpty())
        return FloatSize();

    FloatPoint startPoint = point + WebCore::size(glyphBuffer.initialAdvance());
    drawGlyphBuffer(context, glyphBuffer, startPoint, customFontNotReadyAction);
    return startPoint - point;
}

void FontCascade::drawEmphasisMarks(GraphicsContext& context, const TextRun& run, const AtomString& mark, const FloatPoint& point, unsigned from, std::optional<unsigned> to) const
{
    if (isLoadingCustomFonts())
        return;

    unsigned destination = to.value_or(run.length());

    auto glyphBuffer = layoutText(codePath(run, from, to), run, from, destination, ForTextEmphasisOrNot::ForTextEmphasis);
    glyphBuffer.flatten();

    if (glyphBuffer.isEmpty())
        return;

    FloatPoint startPoint = point + WebCore::size(glyphBuffer.initialAdvance());
    drawEmphasisMarks(context, glyphBuffer, mark, startPoint);
}

RefPtr<const DisplayList::DisplayList> FontCascade::displayListForTextRun(GraphicsContext& context, const TextRun& run, unsigned from, std::optional<unsigned> to, CustomFontNotReadyAction customFontNotReadyAction) const
{
    ASSERT(!context.paintingDisabled());
    unsigned destination = to.value_or(run.length());

    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    CodePath codePathToUse = codePath(run);
    if (codePathToUse != CodePath::Complex && (enableKerning() || requiresShaping()) && (from || destination != run.length()))
        codePathToUse = CodePath::Complex;

    auto glyphBuffer = layoutText(codePathToUse, run, from, destination);
    glyphBuffer.flatten();

    if (glyphBuffer.isEmpty())
        return nullptr;

    DisplayList::RecorderImpl recordingContext(context.state().clone(GraphicsContextState::Purpose::Initial), { },
        context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale), context.colorSpace(),
        DisplayList::Recorder::DrawGlyphsMode::DeconstructAndRetain);

    FloatPoint startPoint = toFloatPoint(WebCore::size(glyphBuffer.initialAdvance()));
    drawGlyphBuffer(recordingContext, glyphBuffer, startPoint, customFontNotReadyAction);

    return recordingContext.takeDisplayList();
}

float FontCascade::widthOfTextRange(const TextRun& run, unsigned from, unsigned to, SingleThreadWeakHashSet<const Font>* fallbackFonts, float* outWidthBeforeRange, float* outWidthAfterRange) const
{
    ASSERT(from <= to);
    ASSERT(to <= run.length());

    if (!run.length())
        return 0;

    float offsetBeforeRange = 0;
    float offsetAfterRange = 0;
    float totalWidth = 0;

    auto codePathToUse = codePath(run);
    if (codePathToUse == CodePath::Complex) {
        ComplexTextController complexIterator(*this, run, false, fallbackFonts);
        complexIterator.advance(from, nullptr, GlyphIterationStyle::IncludePartialGlyphs, fallbackFonts);
        offsetBeforeRange = complexIterator.runWidthSoFar();
        complexIterator.advance(to, nullptr, GlyphIterationStyle::IncludePartialGlyphs, fallbackFonts);
        offsetAfterRange = complexIterator.runWidthSoFar();
        complexIterator.advance(run.length(), nullptr, GlyphIterationStyle::IncludePartialGlyphs, fallbackFonts);
        totalWidth = complexIterator.runWidthSoFar();
    } else {
        WidthIterator simpleIterator(*this, run, fallbackFonts);
        GlyphBuffer glyphBuffer;
        simpleIterator.advance(from, glyphBuffer);
        offsetBeforeRange = simpleIterator.runWidthSoFar();
        simpleIterator.advance(to, glyphBuffer);
        offsetAfterRange = simpleIterator.runWidthSoFar();
        simpleIterator.advance(run.length(), glyphBuffer);
        totalWidth = simpleIterator.runWidthSoFar();
        simpleIterator.finalize(glyphBuffer);
        // FIXME: Finalizing the WidthIterator can affect the total width.
        // We might need to adjust the various widths we've measured to account for that.
    }

    if (outWidthBeforeRange)
        *outWidthBeforeRange = offsetBeforeRange;

    if (outWidthAfterRange)
        *outWidthAfterRange = totalWidth - offsetAfterRange;

    return offsetAfterRange - offsetBeforeRange;
}

float FontCascade::width(StringView text) const
{
    TextRun run { text };
    return width (run);
}

float FontCascade::width(const TextRun& run, SingleThreadWeakHashSet<const Font>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    if (!run.length())
        return 0;

    CodePath codePathToUse = codePath(run);
    if (codePathToUse != CodePath::Complex) {
        // The complex path is more restrictive about returning fallback fonts than the simple path, so we need an explicit test to make their behaviors match.
        if constexpr (!canReturnFallbackFontsForComplexText())
            fallbackFonts = nullptr;
        // The simple path can optimize the case where glyph overflow is not observable.
        if (codePathToUse != CodePath::SimpleWithGlyphOverflow && (glyphOverflow && !glyphOverflow->computeBounds))
            glyphOverflow = nullptr;
    }

    bool hasWordSpacingOrLetterSpacing = wordSpacing() || letterSpacing();
    float* cacheEntry = fonts()->widthCache().add(run, std::numeric_limits<float>::quiet_NaN(), enableKerning() || requiresShaping(), hasWordSpacingOrLetterSpacing, !textAutospace().isNoAutospace(), glyphOverflow);
    if (cacheEntry && !std::isnan(*cacheEntry))
        return *cacheEntry;

    SingleThreadWeakHashSet<const Font> localFallbackFonts;
    if (!fallbackFonts)
        fallbackFonts = &localFallbackFonts;

    float result;
    if (codePathToUse == CodePath::Complex)
        result = widthForComplexText(run, fallbackFonts, glyphOverflow);
    else
        result = widthForSimpleText(run, fallbackFonts, glyphOverflow);

    if (cacheEntry && fallbackFonts->isEmptyIgnoringNullReferences())
        *cacheEntry = result;
    return result;
}
NEVER_INLINE float FontCascade::widthForSimpleTextSlow(StringView text, TextDirection textDirection, float* cacheEntry) const
{
    GlyphBuffer glyphBuffer;
    Ref font = primaryFont();
    ASSERT(!font->syntheticBoldOffset()); // This function should only be called when RenderText::computeCanUseSimplifiedTextMeasuring() returns true, and that function requires no synthetic bold.

    auto addGlyphsFromText = [&](GlyphBuffer& glyphBuffer, const Font& font, auto characters) {
        for (size_t i = 0; i < characters.size(); ++i) {
            auto glyph = font.glyphForCharacter(characters[i]);
            glyphBuffer.add(glyph, font, font.widthForGlyph(glyph), i);
        }
    };

    if (text.is8Bit())
        addGlyphsFromText(glyphBuffer, font, text.span8());
    else
        addGlyphsFromText(glyphBuffer, font, text.span16());

    auto initialAdvance = font->applyTransforms(glyphBuffer, 0, 0, enableKerning(), requiresShaping(), fontDescription().computedLocale(), text, textDirection);
    auto width = 0.f;
    for (size_t i = 0; i < glyphBuffer.size(); ++i)
        width += WebCore::width(glyphBuffer.advanceAt(i));
    width += WebCore::width(initialAdvance);

    if (cacheEntry)
        *cacheEntry = width;
    return width;
}

float FontCascade::widthForSimpleTextWithFixedPitch(StringView text, bool whitespaceIsCollapsed) const
{
    if (text.isEmpty())
        return 0;

    auto monospaceCharacterWidth = primaryFont()->spaceWidth();
    if (whitespaceIsCollapsed)
        return text.length() * monospaceCharacterWidth;

    float* cacheEntry = fonts()->widthCache().add(text, std::numeric_limits<float>::quiet_NaN());
    if (cacheEntry && !std::isnan(*cacheEntry))
        return *cacheEntry;

    auto width = 0.f;
    for (unsigned index = 0; index < text.length(); ++index) {
        auto character = text[index];
        ASSERT(character != tabCharacter); // canUseSimplifiedTextMeasuring will return false for tab character with !whitespaceIsCollapsed.
        if (character == newlineCharacter || character == lineSeparator || character == paragraphSeparator) {
            // Zero width.
        } else if (character >= space)
            width += monospaceCharacterWidth;
        if (index && character == space)
            width += wordSpacing();
    }

    if (cacheEntry)
        *cacheEntry = width;
    return width;
}

float FontCascade::zeroWidth() const
{
    // This represents the advance measure of the glyph 0 (zero, the Unicode character U+0030)
    // in the element's font. In cases where it is impossible or impractical to determine the measure of the 0 glyph,
    // it must be assumed to be 0.5em
    auto defaultZeroWidthValue = fontDescription().computedSize() / 2;
    if (!metricsOfPrimaryFont().zeroWidth())
        return defaultZeroWidthValue;

    auto glyphData = glyphDataForCharacter('0', false);
    if (!glyphData.isValid())
        return defaultZeroWidthValue;
    return glyphData.font->fontMetrics().zeroWidth().value_or(defaultZeroWidthValue);
}

GlyphData FontCascade::glyphDataForCharacter(char32_t c, bool mirror, FontVariant variant, std::optional<ResolvedEmojiPolicy> resolvedEmojiPolicy) const
{
    if (variant == AutoVariant) {
        if (m_fontDescription.variantCaps() == FontVariantCaps::Small) {
            char32_t upperC = u_toupper(c);
            if (upperC != c) {
                c = upperC;
                variant = SmallCapsVariant;
            } else
                variant = NormalVariant;
        } else
            variant = NormalVariant;
    }

    if (mirror)
        c = u_charMirror(c);

    auto emojiPolicy = resolvedEmojiPolicy.value_or(resolveEmojiPolicy(m_fontDescription.variantEmoji(), c));

    return protectedFonts()->glyphDataForCharacter(c, m_fontDescription, protectedFontSelector().get(), variant, emojiPolicy);
}


bool FontCascade::canUseSimplifiedTextMeasuring(char32_t character, FontVariant fontVariant, bool whitespaceIsCollapsed, const Font& primaryFont) const
{
    if (character == tabCharacter && !whitespaceIsCollapsed)
        return false;

    // We cache whitespaceIsCollapsed = true result. false case is handled above.
    whitespaceIsCollapsed = true;
    bool isCacheable = fontVariant == AutoVariant && isLatin1(character);
    size_t baseIndex = static_cast<size_t>(character) * bitsPerCharacterInCanUseSimplifiedTextMeasuringForAutoVariantCache;
    if (isCacheable) {
        static_assert(0 < bitsPerCharacterInCanUseSimplifiedTextMeasuringForAutoVariantCache);
        static_assert(1 < bitsPerCharacterInCanUseSimplifiedTextMeasuringForAutoVariantCache);
        if (m_canUseSimplifiedTextMeasuringForAutoVariantCache.get(baseIndex))
            return m_canUseSimplifiedTextMeasuringForAutoVariantCache.get(baseIndex + 1);
    }

    bool result = WidthIterator::characterCanUseSimplifiedTextMeasuring(character, whitespaceIsCollapsed);
    if (result) {
        constexpr bool mirror = false;
        auto glyphData = glyphDataForCharacter(character, mirror, fontVariant);
        result = glyphData.isValid() && glyphData.font == &primaryFont;
    }

    if (isCacheable) {
        m_canUseSimplifiedTextMeasuringForAutoVariantCache.set(baseIndex, true);
        m_canUseSimplifiedTextMeasuringForAutoVariantCache.set(baseIndex + 1, result);
    }
    return result;
}

// For font families where any of the fonts don't have a valid entry in the OS/2 table
// for avgCharWidth, fallback to the legacy webkit behavior of getting the avgCharWidth
// from the width of a '0'. This only seems to apply to a fixed number of Mac fonts,
// but, in order to get similar rendering across platforms, we do this check for
// all platforms.
bool FontCascade::hasValidAverageCharWidth() const
{
    ASSERT(isMainThread());

    const AtomString& family = firstFamily();
    if (family.isEmpty())
        return false;

#if PLATFORM(COCOA)
    // Internal fonts on macOS and iOS also have an invalid entry in the table for avgCharWidth.
    if (primaryFontIsSystemFont())
        return false;
#endif

    static constexpr ComparableASCIILiteral names[] = {
        "#GungSeo"_s,
        "#HeadLineA"_s,
        "#PCMyungjo"_s,
        "#PilGi"_s,
        "American Typewriter"_s,
        "Apple Braille"_s,
        "Apple LiGothic"_s,
        "Apple LiSung"_s,
        "Apple Symbols"_s,
        "AppleGothic"_s,
        "AppleMyungjo"_s,
        "Arial Hebrew"_s,
        "Chalkboard"_s,
        "Cochin"_s,
        "Corsiva Hebrew"_s,
        "Courier"_s,
        "Euphemia UCAS"_s,
        "Geneva"_s,
        "Gill Sans"_s,
        "Hei"_s,
        "Helvetica"_s,
        "Hoefler Text"_s,
        "InaiMathi"_s,
        "Kai"_s,
        "Lucida Grande"_s,
        "Marker Felt"_s,
        "Monaco"_s,
        "Mshtakan"_s,
        "New Peninim MT"_s,
        "Osaka"_s,
        "Raanana"_s,
        "STHeiti"_s,
        "Symbol"_s,
        "Times"_s,
    };
    static constexpr SortedArraySet set { names };
    return !set.contains(family);
}

bool FontCascade::fastAverageCharWidthIfAvailable(float& width) const
{
    bool success = hasValidAverageCharWidth();
    if (success)
        width = roundf(primaryFont()->avgCharWidth()); // FIXME: primaryFont() might not correspond to firstFamily().
    return success;
}

Vector<LayoutRect> FontCascade::characterSelectionRectsForText(const TextRun& run, const LayoutRect& selectionRect, unsigned from, std::optional<unsigned> toOrEndOfRun) const
{
    unsigned to = toOrEndOfRun.value_or(run.length());
    ASSERT(from <= to);

    bool rtl = run.rtl();

    // FIXME: We could further optimize this by using the simple text codepath when applicable.
    ComplexTextController controller(*this, run);
    controller.advance(from);

    return Vector<LayoutRect>(to - from, [&](size_t i) {
        auto current = from + i + 1;
        auto characterRect = selectionRect;
        auto beforeWidth = controller.runWidthSoFar();

        controller.advance(current);
        auto afterWidth = controller.runWidthSoFar();

        characterRect.move(rtl ? controller.totalAdvance().width() - afterWidth : beforeWidth, 0);
        characterRect.setWidth(LayoutUnit::fromFloatCeil(afterWidth - beforeWidth));
        return characterRect;
    });
}

void FontCascade::adjustSelectionRectForText(bool canUseSimplifiedTextMeasuring, const TextRun& run, LayoutRect& selectionRect, unsigned from, std::optional<unsigned> to) const
{
    unsigned destination = to.value_or(run.length());

    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    CodePath codePathToUse = codePath(run);
    if (codePathToUse != CodePath::Complex) {
        if (canUseSimplifiedTextMeasuring && canTakeFixedPitchFastContentMeasuring())
            return adjustSelectionRectForSimpleTextWithFixedPitch(run, selectionRect, from, destination);

        if ((enableKerning() || requiresShaping()) && (from || destination != run.length()))
            codePathToUse = CodePath::Complex;
    }
    if (codePathToUse != CodePath::Complex)
        return adjustSelectionRectForSimpleText(run, selectionRect, from, destination);

    return adjustSelectionRectForComplexText(run, selectionRect, from, destination);
}

int FontCascade::offsetForPosition(const TextRun& run, float x, bool includePartialGlyphs) const
{
    if (codePath(run, x) != CodePath::Complex)
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);

    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

template <typename CharacterType>
static inline String normalizeSpacesInternal(std::span<const CharacterType> characters)
{
    StringBuilder normalized;
    normalized.reserveCapacity(characters.size());

    for (auto character : characters)
        normalized.append(FontCascade::normalizeSpaces(character));

    return normalized.toString();
}

String FontCascade::normalizeSpaces(std::span<const LChar> characters)
{
    return normalizeSpacesInternal(characters);
}

String FontCascade::normalizeSpaces(std::span<const UChar> characters)
{
    return normalizeSpacesInternal(characters);
}

String FontCascade::normalizeSpaces(StringView stringView)
{
    if (stringView.is8Bit())
        return normalizeSpacesInternal(stringView.span8());
    return normalizeSpacesInternal(stringView.span16());
}

static std::atomic<bool> disableFontSubpixelAntialiasingForTesting = false;

void FontCascade::setDisableFontSubpixelAntialiasingForTesting(bool disable)
{
    ASSERT(isMainThread());
    disableFontSubpixelAntialiasingForTesting = disable;
}

bool FontCascade::shouldDisableFontSubpixelAntialiasingForTesting()
{
    return disableFontSubpixelAntialiasingForTesting;
}

void FontCascade::setCodePath(CodePath p)
{
    s_codePath = p;
}

FontCascade::CodePath FontCascade::codePath()
{
    return s_codePath;
}

FontCascade::CodePath FontCascade::codePath(const TextRun& run, std::optional<unsigned> from, std::optional<unsigned> to) const
{
    if (s_codePath != CodePath::Auto)
        return s_codePath;

#if !USE(FREETYPE)
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if ((enableKerning() || requiresShaping()) && (from.value_or(0) || to.value_or(run.length()) != run.length()))
        return CodePath::Complex;
#else
    UNUSED_PARAM(from);
    UNUSED_PARAM(to);
#endif

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=150791: @font-face features should also cause this to be complex.

#if !USE(FONT_VARIANT_VIA_FEATURES) && !USE(FREETYPE)
    if (run.length() > 1 && (enableKerning() || requiresShaping()))
        return CodePath::Complex;
#endif

    if (!run.characterScanForCodePath())
        return CodePath::Simple;

    if (run.is8Bit())
        return CodePath::Simple;

    // Start from 0 since drawing and highlighting also measure the characters before run->from.
    return characterRangeCodePath(run.span16());
}

FontCascade::CodePath FontCascade::characterRangeCodePath(std::span<const UChar> span)
{
    // FIXME: Should use a UnicodeSet in ports where ICU is used. Note that we 
    // can't simply use UnicodeCharacter Property/class because some characters
    // are not 'combining', but still need to go to the complex path.
    // Alternatively, we may as well consider binary search over a sorted
    // list of ranges.
    CodePath result = CodePath::Simple;
    bool previousCharacterIsEmojiGroupCandidate = false;
    size_t size = span.size();
    for (size_t i = 0; i < size; ++i) {
        auto c = span[i];
        if (c == zeroWidthJoiner && previousCharacterIsEmojiGroupCandidate)
            return CodePath::Complex;
        
        previousCharacterIsEmojiGroupCandidate = false;
        if (c < 0x2E5) // U+02E5 through U+02E9 (Modifier Letters : Tone letters)  
            continue;
        if (c <= 0x2E9) 
            return CodePath::Complex;

        if (c < 0x300) // U+0300 through U+036F Combining diacritical marks
            continue;
        if (c <= 0x36F)
            return CodePath::Complex;

        if (c < 0x0591 || c == 0x05BE) // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
            continue;
        if (c <= 0x05CF)
            return CodePath::Complex;

        // U+0600 through U+109F Arabic, Syriac, Thaana, NKo, Samaritan, Mandaic,
        // Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, 
        // Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
        if (c < 0x0600) 
            continue;
        if (c <= 0x109F)
            return CodePath::Complex;

        // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose;
        // Modern Korean will be precomposed as a result of step A)
        if (c < 0x1100)
            continue;
        if (c <= 0x11FF)
            return CodePath::Complex;

        if (c < 0x135D) // U+135D through U+135F Ethiopic combining marks
            continue;
        if (c <= 0x135F)
            return CodePath::Complex;

        if (c < 0x1700) // U+1780 through U+18AF Tagalog, Hanunoo, Buhid, Taghanwa,Khmer, Mongolian
            continue;
        if (c <= 0x18AF)
            return CodePath::Complex;

        if (c < 0x1900) // U+1900 through U+194F Limbu (Unicode 4.0)
            continue;
        if (c <= 0x194F)
            return CodePath::Complex;

        if (c < 0x1980) // U+1980 through U+19DF New Tai Lue
            continue;
        if (c <= 0x19DF)
            return CodePath::Complex;

        if (c < 0x1A00) // U+1A00 through U+1CFF Buginese, Tai Tham, Balinese, Batak, Lepcha, Vedic
            continue;
        if (c <= 0x1CFF)
            return CodePath::Complex;

        if (c < 0x1DC0) // U+1DC0 through U+1DFF Comining diacritical mark supplement
            continue;
        if (c <= 0x1DFF)
            return CodePath::Complex;

        // U+1E00 through U+2000 characters with diacritics and stacked diacritics
        if (c <= 0x2000) {
            result = CodePath::SimpleWithGlyphOverflow;
            continue;
        }

        if (c < 0x20D0) // U+20D0 through U+20FF Combining marks for symbols
            continue;
        if (c <= 0x20FF)
            return CodePath::Complex;

        if (c < 0x26F9)
            continue;
        if (c < 0x26FA)
            return CodePath::Complex;

        if (c < 0x2CEF) // U+2CEF through U+2CF1 Combining marks for Coptic
            continue;
        if (c <= 0x2CF1)
            return CodePath::Complex;

        if (c < 0x302A) // U+302A through U+302F Ideographic and Hangul Tone marks
            continue;
        if (c <= 0x302F)
            return CodePath::Complex;

        if (c < 0x3099)
            continue;
        if (c < 0x309D)
            return CodePath::Complex; // KATAKANA-HIRAGANA (SEMI-)VOICED SOUND MARKS require character composition

        if (c < 0xA67C) // U+A67C through U+A67D Combining marks for old Cyrillic
            continue;
        if (c <= 0xA67D)
            return CodePath::Complex;

        if (c < 0xA6F0) // U+A6F0 through U+A6F1 Combining mark for Bamum
            continue;
        if (c <= 0xA6F1)
            return CodePath::Complex;

        // U+A800 through U+ABFF Nagri, Phags-pa, Saurashtra, Devanagari Extended,
        // Hangul Jamo Ext. A, Javanese, Myanmar Extended A, Tai Viet, Meetei Mayek,
        if (c < 0xA800) 
            continue;
        if (c <= 0xABFF)
            return CodePath::Complex;

        if (c < 0xD7B0) // U+D7B0 through U+D7FF Hangul Jamo Ext. B
            continue;
        if (c <= 0xD7FF)
            return CodePath::Complex;

        if (c <= 0xDBFF) {
            // High surrogate

            if (i + 1 == size)
                continue;

            UChar next = span[++i];
            if (!U16_IS_TRAIL(next))
                continue;

            char32_t supplementaryCharacter = U16_GET_SUPPLEMENTARY(c, next);

            if (supplementaryCharacter < 0x10A00)
                continue;
            if (supplementaryCharacter < 0x10A60) // Kharoshthi
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11000)
                continue;
            if (supplementaryCharacter < 0x11080) // Brahmi
                return CodePath::Complex;
            if (supplementaryCharacter < 0x110D0) // Kaithi
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11100)
                continue;
            if (supplementaryCharacter < 0x11150) // Chakma
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11180) // Mahajani
                return CodePath::Complex;
            if (supplementaryCharacter < 0x111E0) // Sharada
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11200)
                continue;
            if (supplementaryCharacter < 0x11250) // Khojki
                return CodePath::Complex;
            if (supplementaryCharacter < 0x112B0)
                continue;
            if (supplementaryCharacter < 0x11300) // Khudawadi
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11380) // Grantha
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11400)
                continue;
            if (supplementaryCharacter < 0x11480) // Newa
                return CodePath::Complex;
            if (supplementaryCharacter < 0x114E0) // Tirhuta
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11580)
                continue;
            if (supplementaryCharacter < 0x11600) // Siddham
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11660) // Modi
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11680)
                continue;
            if (supplementaryCharacter < 0x116D0) // Takri
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11700)
                continue;
            if (supplementaryCharacter < 0x11C00) // Ahom, Dogra, Dives Akuru, Nandinagari, Zanabazar Square, Soyombo, Warang Citi, Pau Cin Hau
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11C70) // Bhaiksuki
                return CodePath::Complex;
            if (supplementaryCharacter < 0x11CC0) // Marchen
                return CodePath::Complex;
            if (supplementaryCharacter < 0x1E900)
                continue;
            if (supplementaryCharacter < 0x1E960) // Adlam
                return CodePath::Complex;
            if (supplementaryCharacter < 0x1F1E6) // U+1F1E6 through U+1F1FF Regional Indicator Symbols
                continue;
            if (supplementaryCharacter <= 0x1F1FF)
                return CodePath::Complex;

            if (isEmojiFitzpatrickModifier(supplementaryCharacter))
                return CodePath::Complex;
            if (isEmojiGroupCandidate(supplementaryCharacter)) {
                previousCharacterIsEmojiGroupCandidate = true;
                continue;
            }

            if (supplementaryCharacter < 0xE0000)
                continue;
            if (supplementaryCharacter < 0xE0080) // Tags
                return CodePath::Complex;
            if (supplementaryCharacter < 0xE0100) // U+E0100 through U+E01EF Unicode variation selectors.
                continue;
            if (supplementaryCharacter <= 0xE01EF)
                return CodePath::Complex;

            // FIXME: Check for Brahmi (U+11000 block), Kaithi (U+11080 block) and other complex scripts
            // in plane 1 or higher.

            continue;
        }

        if (c < 0xFE00) // U+FE00 through U+FE0F Unicode variation selectors
            continue;
        if (c <= 0xFE0F)
            return CodePath::Complex;

        if (c < 0xFE20) // U+FE20 through U+FE2F Combining half marks
            continue;
        if (c <= 0xFE2F)
            return CodePath::Complex;
    }
    return result;
}

bool FontCascade::isCJKIdeograph(char32_t c)
{
    // The basic CJK Unified Ideographs block.
    if (c >= 0x4E00 && c <= 0x9FFF)
        return true;
    
    // CJK Unified Ideographs Extension A.
    if (c >= 0x3400 && c <= 0x4DBF)
        return true;
    
    // CJK Radicals Supplement.
    if (c >= 0x2E80 && c <= 0x2EFF)
        return true;
    
    // Kangxi Radicals.
    if (c >= 0x2F00 && c <= 0x2FDF)
        return true;
    
    // CJK Strokes.
    if (c >= 0x31C0 && c <= 0x31EF)
        return true;
    
    // CJK Compatibility Ideographs.
    if (c >= 0xF900 && c <= 0xFAFF)
        return true;

    // CJK Unified Ideographs Extension B.
    if (c >= 0x20000 && c <= 0x2A6DF)
        return true;

    // CJK Unified Ideographs Extension C.
    if (c >= 0x2A700 && c <= 0x2B73F)
        return true;
    
    // CJK Unified Ideographs Extension D.
    if (c >= 0x2B740 && c <= 0x2B81F)
        return true;
    
    // CJK Compatibility Ideographs Supplement.
    if (c >= 0x2F800 && c <= 0x2FA1F)
        return true;

    return false;
}

bool FontCascade::isCJKIdeographOrSymbol(char32_t c)
{
    // 0x2C7 Caron, Mandarin Chinese 3rd Tone
    // 0x2CA Modifier Letter Acute Accent, Mandarin Chinese 2nd Tone
    // 0x2CB Modifier Letter Grave Access, Mandarin Chinese 4th Tone
    // 0x2D9 Dot Above, Mandarin Chinese 5th Tone
    // 0x2EA Modifier Letter Yin Departing Tone Mark
    // 0x2EB Modifier Letter Yang Departing Tone Mark
    if ((c == 0x2C7) || (c == 0x2CA) || (c == 0x2CB) || (c == 0x2D9) || (c == 0x2EA) || (c == 0x2EB))
        return true;

    if ((c == 0x2020) || (c == 0x2021) || (c == 0x2030) || (c == 0x203B) || (c == 0x203C)
        || (c == 0x2042) || (c == 0x2047) || (c == 0x2048) || (c == 0x2049) || (c == 0x2051)
        || (c == 0x20DD) || (c == 0x20DE) || (c == 0x2100) || (c == 0x2103) || (c == 0x2105)
        || (c == 0x2109) || (c == 0x210A) || (c == 0x2113) || (c == 0x2116) || (c == 0x2121)
        || (c == 0x212B) || (c == 0x213B) || (c == 0x2150) || (c == 0x2151) || (c == 0x2152))
        return true;

    if (c >= 0x2156 && c <= 0x215A)
        return true;

    if (c >= 0x2160 && c <= 0x216B)
        return true;

    if (c >= 0x2170 && c <= 0x217B)
        return true;

    if ((c == 0x217F) || (c == 0x2189) || (c == 0x2307) || (c == 0x2312) || (c == 0x23BE) || (c == 0x23BF))
        return true;

    if (c >= 0x23C0 && c <= 0x23CC)
        return true;

    if ((c == 0x23CE) || (c == 0x2423))
        return true;

    if (c >= 0x2460 && c <= 0x2492)
        return true;

    if (c >= 0x249C && c <= 0x24FF)
        return true;

    if ((c == 0x25A0) || (c == 0x25A1) || (c == 0x25A2) || (c == 0x25AA) || (c == 0x25AB))
        return true;

    if ((c == 0x25B1) || (c == 0x25B2) || (c == 0x25B3) || (c == 0x25B6) || (c == 0x25B7) || (c == 0x25BC) || (c == 0x25BD))
        return true;
    
    if ((c == 0x25C0) || (c == 0x25C1) || (c == 0x25C6) || (c == 0x25C7) || (c == 0x25C9) || (c == 0x25CB) || (c == 0x25CC))
        return true;

    if (c >= 0x25CE && c <= 0x25D3)
        return true;

    if (c >= 0x25E2 && c <= 0x25E6)
        return true;

    if (c == 0x25EF)
        return true;

    if (c >= 0x2600 && c <= 0x2603)
        return true;

    if ((c == 0x2605) || (c == 0x2606) || (c == 0x260E) || (c == 0x2616) || (c == 0x2617) || (c == 0x2640) || (c == 0x2642))
        return true;

    if (c >= 0x2660 && c <= 0x266F)
        return true;

    if (c >= 0x2672 && c <= 0x267D)
        return true;

    if ((c == 0x26A0) || (c == 0x26BD) || (c == 0x26BE) || (c == 0x2713) || (c == 0x271A) || (c == 0x273F) || (c == 0x2740) || (c == 0x2756))
        return true;

    if (c >= 0x2776 && c <= 0x277F)
        return true;

    if (c == 0x2B1A)
        return true;

    // Ideographic Description Characters.
    if (c >= 0x2FF0 && c <= 0x2FFF)
        return true;

    // CJK Symbols and Punctuation, excluding 0x3030.
    if (c >= 0x3000 && c < 0x3030)
        return true;

    if (c > 0x3030 && c <= 0x303F)
        return true;

    // Hiragana
    if (c >= 0x3040 && c <= 0x309F)
        return true;

    // Katakana 
    if (c >= 0x30A0 && c <= 0x30FF)
        return true;

    // Bopomofo
    if (c >= 0x3100 && c <= 0x312F)
        return true;

    if (c >= 0x3190 && c <= 0x319F)
        return true;

    // Bopomofo Extended
    if (c >= 0x31A0 && c <= 0x31BF)
        return true;

    // Enclosed CJK Letters and Months.
    if (c >= 0x3200 && c <= 0x32FF)
        return true;
    
    // CJK Compatibility.
    if (c >= 0x3300 && c <= 0x33FF)
        return true;

    if (c >= 0xF860 && c <= 0xF862)
        return true;

    // CJK Compatibility Forms.
    if (c >= 0xFE30 && c <= 0xFE4F)
        return true;

    if ((c == 0xFE10) || (c == 0xFE11) || (c == 0xFE12) || (c == 0xFE19))
        return true;

    if ((c == 0xFF0D) || (c == 0xFF1B) || (c == 0xFF1C) || (c == 0xFF1E))
        return false;

    // Halfwidth and Fullwidth Forms
    // Usually only used in CJK
    if (c >= 0xFF00 && c <= 0xFFEF)
        return true;

    // Emoji.
    if (c == 0x1F100)
        return true;

    if (c >= 0x1F110 && c <= 0x1F129)
        return true;

    if (c >= 0x1F130 && c <= 0x1F149)
        return true;

    if (c >= 0x1F150 && c <= 0x1F169)
        return true;

    if (c >= 0x1F170 && c <= 0x1F189)
        return true;

    if (c >= 0x1F200 && c <= 0x1F6C5)
        return true;

    return isCJKIdeograph(c);
}

std::pair<unsigned, bool> FontCascade::expansionOpportunityCountInternal(std::span<const LChar> characters, TextDirection direction, ExpansionBehavior expansionBehavior)
{
    unsigned count = 0;
    bool isAfterExpansion = expansionBehavior.left == ExpansionBehavior::Behavior::Forbid;
    if (expansionBehavior.left == ExpansionBehavior::Behavior::Force) {
        ++count;
        isAfterExpansion = true;
    }
    if (direction == TextDirection::LTR) {
        for (auto character : characters) {
            if (treatAsSpace(character)) {
                ++count;
                isAfterExpansion = true;
            } else
                isAfterExpansion = false;
        }
    } else {
        for (auto character : makeReversedRange(characters)) {
            if (treatAsSpace(character)) {
                ++count;
                isAfterExpansion = true;
            } else
                isAfterExpansion = false;
        }
    }
    if (!isAfterExpansion && expansionBehavior.right == ExpansionBehavior::Behavior::Force) {
        ++count;
        isAfterExpansion = true;
    } else if (isAfterExpansion && expansionBehavior.right == ExpansionBehavior::Behavior::Forbid) {
        ASSERT(count);
        --count;
        isAfterExpansion = false;
    }
    return std::make_pair(count, isAfterExpansion);
}

std::pair<unsigned, bool> FontCascade::expansionOpportunityCountInternal(std::span<const UChar> characters, TextDirection direction, ExpansionBehavior expansionBehavior)
{
    unsigned count = 0;
    bool isAfterExpansion = expansionBehavior.left == ExpansionBehavior::Behavior::Forbid;
    if (expansionBehavior.left == ExpansionBehavior::Behavior::Force) {
        ++count;
        isAfterExpansion = true;
    }
    if (direction == TextDirection::LTR) {
        for (size_t i = 0; i < characters.size(); ++i) {
            char32_t character = characters[i];
            if (treatAsSpace(character)) {
                ++count;
                isAfterExpansion = true;
                continue;
            }
            if (U16_IS_LEAD(character) && i + 1 < characters.size() && U16_IS_TRAIL(characters[i + 1])) {
                character = U16_GET_SUPPLEMENTARY(character, characters[i + 1]);
                ++i;
            }
            if (canExpandAroundIdeographsInComplexText() && isCJKIdeographOrSymbol(character)) {
                if (!isAfterExpansion)
                    ++count;
                ++count;
                isAfterExpansion = true;
                continue;
            }
            isAfterExpansion = false;
        }
    } else {
        for (size_t i = characters.size(); i > 0; --i) {
            char32_t character = characters[i - 1];
            if (treatAsSpace(character)) {
                ++count;
                isAfterExpansion = true;
                continue;
            }
            if (U16_IS_TRAIL(character) && i > 1 && U16_IS_LEAD(characters[i - 2])) {
                character = U16_GET_SUPPLEMENTARY(characters[i - 2], character);
                --i;
            }
            if (canExpandAroundIdeographsInComplexText() && isCJKIdeographOrSymbol(character)) {
                if (!isAfterExpansion)
                    ++count;
                ++count;
                isAfterExpansion = true;
                continue;
            }
            isAfterExpansion = false;
        }
    }
    if (!isAfterExpansion && expansionBehavior.right == ExpansionBehavior::Behavior::Force) {
        ++count;
        isAfterExpansion = true;
    } else if (isAfterExpansion && expansionBehavior.right == ExpansionBehavior::Behavior::Forbid) {
        ASSERT(count);
        --count;
        isAfterExpansion = false;
    }
    return std::make_pair(count, isAfterExpansion);
}

std::pair<unsigned, bool> FontCascade::expansionOpportunityCount(StringView stringView, TextDirection direction, ExpansionBehavior expansionBehavior)
{
    // For each character, iterating from left to right:
    //   If it is recognized as a space, insert an opportunity after it
    //   If it is an ideograph, insert one opportunity before it and one opportunity after it
    // Do this such a way so that there are not two opportunities next to each other.
    if (stringView.is8Bit())
        return expansionOpportunityCountInternal(stringView.span8(), direction, expansionBehavior);
    return expansionOpportunityCountInternal(stringView.span16(), direction, expansionBehavior);
}

bool FontCascade::leftExpansionOpportunity(StringView stringView, TextDirection direction)
{
    if (!stringView.length())
        return false;

    char32_t initialCharacter;
    if (direction == TextDirection::LTR) {
        initialCharacter = stringView[0];
        if (U16_IS_LEAD(initialCharacter) && stringView.length() > 1 && U16_IS_TRAIL(stringView[1]))
            initialCharacter = U16_GET_SUPPLEMENTARY(initialCharacter, stringView[1]);
    } else {
        initialCharacter = stringView[stringView.length() - 1];
        if (U16_IS_TRAIL(initialCharacter) && stringView.length() > 1 && U16_IS_LEAD(stringView[stringView.length() - 2]))
            initialCharacter = U16_GET_SUPPLEMENTARY(stringView[stringView.length() - 2], initialCharacter);
    }

    return canExpandAroundIdeographsInComplexText() && isCJKIdeographOrSymbol(initialCharacter);
}

bool FontCascade::rightExpansionOpportunity(StringView stringView, TextDirection direction)
{
    if (!stringView.length())
        return false;

    char32_t finalCharacter;
    if (direction == TextDirection::LTR) {
        finalCharacter = stringView[stringView.length() - 1];
        if (U16_IS_TRAIL(finalCharacter) && stringView.length() > 1 && U16_IS_LEAD(stringView[stringView.length() - 2]))
            finalCharacter = U16_GET_SUPPLEMENTARY(stringView[stringView.length() - 2], finalCharacter);
    } else {
        finalCharacter = stringView[0];
        if (U16_IS_LEAD(finalCharacter) && stringView.length() > 1 && U16_IS_TRAIL(stringView[1]))
            finalCharacter = U16_GET_SUPPLEMENTARY(finalCharacter, stringView[1]);
    }

    return treatAsSpace(finalCharacter) || (canExpandAroundIdeographsInComplexText() && isCJKIdeographOrSymbol(finalCharacter));
}

// https://www.w3.org/TR/css-text-decor-3/#text-emphasis-style-property
bool FontCascade::canReceiveTextEmphasis(char32_t c)
{
    auto mask = U_GET_GC_MASK(c);
    if (mask & (U_GC_Z_MASK | U_GC_CN_MASK | U_GC_CC_MASK | U_GC_CF_MASK))
        return false;

    // Additional word-separator characters listed in CSS Text Level 3 Editor's Draft 3 November 2010.
    // https://www.w3.org/TR/css-text-3/#word-separator
    if (c == ethiopicWordspace || c == aegeanWordSeparatorLine || c == aegeanWordSeparatorDot
        || c == ugariticWordDivider || c == tibetanMarkIntersyllabicTsheg || c == tibetanMarkDelimiterTshegBstar)
        return false;

    if (mask & U_GC_P_MASK) {
        return c == '#' || c == '%' || c == '&' || c == '@'
            || c == arabicIndicPerMilleSign
            || c == arabicIndicPerTenThousandSign
            || c == arabicPercentSign
            || c == fullwidthAmpersand
            || c == fullwidthCommercialAt
            || c == fullwidthNumberSign
            || c == fullwidthPercentSign
            || c == partAlternationMark
            || c == perMilleSign
            || c == perTenThousandSign
            || c == pilcrowSign
            || c == reversedPilcrowSign
            || c == sectionSign
            || c == smallAmpersand
            || c == smallCommercialAt
            || c == smallNumberSign
            || c == smallPercentSign
            || c == swungDash
            || c == tironianSignEt;
    }

    return true;
}

bool FontCascade::isLoadingCustomFonts() const
{
    RefPtr fonts = m_fonts;
    return fonts && fonts->isLoadingCustomFonts();
}

bool FontCascade::computeUseBackslashAsYenSymbol() const
{
    return FontCache::forCurrentThread()->useBackslashAsYenSignForFamily(m_fontDescription.firstFamily());
}

enum class GlyphUnderlineType : uint8_t {
    SkipDescenders,
    SkipGlyph,
    DrawOverGlyph
};
    
static GlyphUnderlineType computeUnderlineType(const TextRun& textRun, const GlyphBuffer& glyphBuffer, unsigned index)
{
    // In general, we want to skip descenders. However, skipping descenders on CJK characters leads to undesirable renderings,
    // so we want to draw through CJK characters (on a character-by-character basis).
    // FIXME: The CSS spec says this should instead be done by the user-agent stylesheet using the lang= attribute.
    char32_t baseCharacter;
    auto offsetInString = glyphBuffer.checkedStringOffsetAt(index, textRun.length());
    if (!offsetInString)
        return GlyphUnderlineType::SkipDescenders;
    
    if (textRun.is8Bit())
        baseCharacter = textRun.span8()[offsetInString.value()];
    else {
        auto characters = textRun.span16();
        U16_GET(characters, 0, static_cast<unsigned>(offsetInString.value()), characters.size(), baseCharacter);
    }
    // u_getIntPropertyValue with UCHAR_IDEOGRAPHIC doesn't return true for Japanese or Korean codepoints.
    // Instead, we can use the "Unicode allocation block" for the character.
    UBlockCode blockCode = ublock_getCode(baseCharacter);
    switch (blockCode) {
    case UBLOCK_CJK_RADICALS_SUPPLEMENT:
    case UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION:
    case UBLOCK_ENCLOSED_CJK_LETTERS_AND_MONTHS:
    case UBLOCK_CJK_COMPATIBILITY:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_A:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS:
    case UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS:
    case UBLOCK_CJK_COMPATIBILITY_FORMS:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_B:
    case UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS_SUPPLEMENT:
    case UBLOCK_CJK_STROKES:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_C:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_D:
    case UBLOCK_IDEOGRAPHIC_DESCRIPTION_CHARACTERS:
    case UBLOCK_LINEAR_B_IDEOGRAMS:
    case UBLOCK_ENCLOSED_IDEOGRAPHIC_SUPPLEMENT:
    case UBLOCK_HIRAGANA:
    case UBLOCK_KATAKANA:
    case UBLOCK_BOPOMOFO:
    case UBLOCK_BOPOMOFO_EXTENDED:
    case UBLOCK_HANGUL_JAMO:
    case UBLOCK_HANGUL_COMPATIBILITY_JAMO:
    case UBLOCK_HANGUL_SYLLABLES:
    case UBLOCK_HANGUL_JAMO_EXTENDED_A:
    case UBLOCK_HANGUL_JAMO_EXTENDED_B:
        return GlyphUnderlineType::DrawOverGlyph;
    default:
        return GlyphUnderlineType::SkipDescenders;
    }
}

// FIXME: This function may not work if the emphasis mark uses a complex script, but none of the
// standard emphasis marks do so.
std::optional<GlyphData> FontCascade::getEmphasisMarkGlyphData(const AtomString& mark) const
{
    if (mark.isEmpty())
        return std::nullopt;

    char32_t character;
    if (!mark.is8Bit()) {
        size_t i = 0;
        auto span = mark.span16();
        U16_NEXT(span, i, span.size(), character);
        ASSERT(U16_IS_SINGLE(character)); // The CSS parser replaces unpaired surrogates with the object replacement character.
    } else
        character = mark[0];

    std::optional<GlyphData> glyphData(glyphDataForCharacter(character, false, EmphasisMarkVariant));
    return glyphData.value().isValid() ? glyphData : std::nullopt;
}

int FontCascade::emphasisMarkAscent(const AtomString& mark) const
{
    std::optional<GlyphData> markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return 0;

    RefPtr markFontData = markGlyphData.value().font.get();
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().intAscent();
}

int FontCascade::emphasisMarkDescent(const AtomString& mark) const
{
    std::optional<GlyphData> markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return 0;

    RefPtr markFontData = markGlyphData.value().font.get();
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().intDescent();
}

const Font* FontCascade::fontForEmphasisMark(const AtomString& mark) const
{
    auto markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return { };

    ASSERT(markGlyphData->font);
    return markGlyphData->font.get();
}

int FontCascade::emphasisMarkHeight(const AtomString& mark) const
{
    if (RefPtr font = fontForEmphasisMark(mark))
        return font->fontMetrics().intHeight();
    return { };
}

float FontCascade::floatEmphasisMarkHeight(const AtomString& mark) const
{
    if (RefPtr font = fontForEmphasisMark(mark))
        return font->fontMetrics().height();
    return { };
}

GlyphBuffer FontCascade::layoutSimpleText(const TextRun& run, unsigned from, unsigned to, ForTextEmphasisOrNot forTextEmphasis) const
{
    GlyphBuffer glyphBuffer;

    WidthIterator it(*this, run, 0, false, forTextEmphasis == ForTextEmphasisOrNot::ForTextEmphasis);
    // FIXME: Using separate glyph buffers for the prefix and the suffix is incorrect when kerning or
    // ligatures are enabled.
    GlyphBuffer localGlyphBuffer;
    it.advance(from, localGlyphBuffer);
    float beforeWidth = it.runWidthSoFar();
    it.advance(to, glyphBuffer);

    if (glyphBuffer.isEmpty())
        return glyphBuffer;

    float afterWidth = it.runWidthSoFar();

    float initialAdvance = 0;
    if (run.rtl()) {
        it.advance(run.length(), localGlyphBuffer);
        it.finalize(localGlyphBuffer);
        initialAdvance = it.runWidthSoFar() - afterWidth;
    } else {
        it.finalize(localGlyphBuffer);
        initialAdvance = beforeWidth;
    }
    glyphBuffer.expandInitialAdvance(initialAdvance);

    // The glyph buffer is currently in logical order,
    // but we need to return the results in visual order.
    if (run.rtl())
        glyphBuffer.reverse(0, glyphBuffer.size());

    return glyphBuffer;
}

GlyphBuffer FontCascade::layoutComplexText(const TextRun& run, unsigned from, unsigned to, ForTextEmphasisOrNot forTextEmphasis) const
{
    GlyphBuffer glyphBuffer;

    ComplexTextController controller(*this, run, false, 0, forTextEmphasis == ForTextEmphasisOrNot::ForTextEmphasis);
    GlyphBuffer glyphBufferForStartingIndex;
    controller.advance(from, &glyphBufferForStartingIndex);
    controller.advance(to, &glyphBuffer);

    if (glyphBuffer.isEmpty())
        return glyphBuffer;

    if (run.rtl()) {
        // Exploit the fact that the sum of the paint advances is equal to
        // the sum of the layout advances.
        FloatSize initialAdvance = controller.totalAdvance();
        for (unsigned i = 0; i < glyphBufferForStartingIndex.size(); ++i)
            initialAdvance -= WebCore::size(glyphBufferForStartingIndex.advanceAt(i));
        for (unsigned i = 0; i < glyphBuffer.size(); ++i)
            initialAdvance -= WebCore::size(glyphBuffer.advanceAt(i));
        // FIXME: Shouldn't we subtract the other initial advance?
        glyphBuffer.reverse(0, glyphBuffer.size());
        glyphBuffer.setInitialAdvance(makeGlyphBufferAdvance(initialAdvance));
    } else {
        FloatSize initialAdvance = WebCore::size(glyphBufferForStartingIndex.initialAdvance());
        for (unsigned i = 0; i < glyphBufferForStartingIndex.size(); ++i)
            initialAdvance += WebCore::size(glyphBufferForStartingIndex.advanceAt(i));
        // FIXME: Shouldn't we add the other initial advance?
        glyphBuffer.setInitialAdvance(makeGlyphBufferAdvance(initialAdvance));
    }

    return glyphBuffer;
}

inline bool shouldDrawIfLoading(const Font& font, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
    // Don't draw anything while we are using custom fonts that are in the process of loading,
    // except if the 'customFontNotReadyAction' argument is set to UseFallbackIfFontNotReady
    // (in which case "font" will be a fallback font).
    return !font.isInterstitial() || font.visibility() == Font::Visibility::Visible || customFontNotReadyAction == FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady;
}

// This function assumes the GlyphBuffer's initial advance has already been incorporated into the start point.
void FontCascade::drawGlyphBuffer(GraphicsContext& context, const GlyphBuffer& glyphBuffer, FloatPoint& point, CustomFontNotReadyAction customFontNotReadyAction) const
{
    ASSERT(glyphBuffer.isFlattened());
    RefPtr fontData = glyphBuffer.fontAt(0);
    FloatPoint startPoint = point;
    float nextX = startPoint.x() + WebCore::width(glyphBuffer.advanceAt(0));
    float nextY = startPoint.y() + height(glyphBuffer.advanceAt(0));
    unsigned lastFrom = 0;
    unsigned nextGlyph = 1;
    while (nextGlyph < glyphBuffer.size()) {
        RefPtr nextFontData = glyphBuffer.fontAt(nextGlyph);

        if (nextFontData != fontData) {
            if (shouldDrawIfLoading(*fontData, customFontNotReadyAction)) {
                size_t glyphCount = nextGlyph - lastFrom;
                context.drawGlyphs(*fontData, glyphBuffer.glyphs(lastFrom, glyphCount), glyphBuffer.advances(lastFrom, glyphCount), startPoint, m_fontDescription.usedFontSmoothing());
            }
            lastFrom = nextGlyph;
            fontData = WTFMove(nextFontData);
            startPoint.setX(nextX);
            startPoint.setY(nextY);
        }
        nextX += WebCore::width(glyphBuffer.advanceAt(nextGlyph));
        nextY += height(glyphBuffer.advanceAt(nextGlyph));
        nextGlyph++;
    }

    if (shouldDrawIfLoading(*fontData, customFontNotReadyAction)) {
        size_t glyphCount = nextGlyph - lastFrom;
        context.drawGlyphs(*fontData, glyphBuffer.glyphs(lastFrom, glyphCount), glyphBuffer.advances(lastFrom, glyphCount), startPoint, m_fontDescription.usedFontSmoothing());
    }
    point.setX(nextX);
}

inline static float offsetToMiddleOfGlyph(const Font& fontData, Glyph glyph)
{
    if (fontData.platformData().orientation() == FontOrientation::Horizontal) {
        FloatRect bounds = fontData.boundsForGlyph(glyph);
        return bounds.x() + bounds.width() / 2;
    }
    // FIXME: Use glyph bounds once they make sense for vertical fonts.
    return fontData.widthForGlyph(glyph) / 2;
}

inline static float offsetToMiddleOfGlyphAtIndex(const GlyphBuffer& glyphBuffer, unsigned i)
{
    return offsetToMiddleOfGlyph(glyphBuffer.protectedFontAt(i), glyphBuffer.glyphAt(i));
}

void FontCascade::drawEmphasisMarks(GraphicsContext& context, const GlyphBuffer& glyphBuffer, const AtomString& mark, const FloatPoint& point) const
{
    ASSERT(glyphBuffer.isFlattened());
    std::optional<GlyphData> markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return;

    RefPtr markFontData = markGlyphData.value().font.get();
    ASSERT(markFontData);
    if (!markFontData)
        return;

    Glyph markGlyph = markGlyphData.value().glyph;
    Glyph spaceGlyph = markFontData->spaceGlyph();

    // FIXME: This needs to take the initial advance into account.
    // The problem might actually be harder for complex text, though.
    // Putting a mark over every glyph probably isn't great in complex scripts.
    float middleOfLastGlyph = offsetToMiddleOfGlyphAtIndex(glyphBuffer, 0);
    FloatPoint startPoint(point.x() + middleOfLastGlyph - offsetToMiddleOfGlyph(*markFontData, markGlyph), point.y());

    GlyphBuffer markBuffer;
    auto glyphForMarker = [&](unsigned index) {
        auto glyph = glyphBuffer.glyphAt(index);
        return (glyph && glyph != deletedGlyph) ? markGlyph : spaceGlyph;
    };

    for (unsigned i = 0; i + 1 < glyphBuffer.size(); ++i) {
        float middleOfNextGlyph = offsetToMiddleOfGlyphAtIndex(glyphBuffer, i + 1);
        float advance = WebCore::width(glyphBuffer.advanceAt(i)) - middleOfLastGlyph + middleOfNextGlyph;
        markBuffer.add(glyphForMarker(i), *markFontData, advance);
        middleOfLastGlyph = middleOfNextGlyph;
    }
    markBuffer.add(glyphForMarker(glyphBuffer.size() - 1), *markFontData, 0);

    drawGlyphBuffer(context, markBuffer, startPoint, CustomFontNotReadyAction::DoNotPaintIfFontNotReady);
}

float FontCascade::widthForSimpleText(const TextRun& run, SingleThreadWeakHashSet<const Font>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    WidthIterator it(*this, run, fallbackFonts, glyphOverflow);
    GlyphBuffer glyphBuffer;
    it.advance(run.length(), glyphBuffer);
    it.finalize(glyphBuffer);

    if (glyphOverflow) {
        glyphOverflow->top = std::max<double>(glyphOverflow->top, -it.minGlyphBoundingBoxY() - (glyphOverflow->computeBounds ? 0 : metricsOfPrimaryFont().ascent()));
        glyphOverflow->bottom = std::max<double>(glyphOverflow->bottom, it.maxGlyphBoundingBoxY() - (glyphOverflow->computeBounds ? 0 : metricsOfPrimaryFont().descent()));
        glyphOverflow->left = it.firstGlyphOverflow();
        glyphOverflow->right = it.lastGlyphOverflow();
    }

    return it.runWidthSoFar();
}

float FontCascade::widthForComplexText(const TextRun& run, SingleThreadWeakHashSet<const Font>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    ComplexTextController controller(*this, run, true, fallbackFonts);
    if (glyphOverflow) {
        glyphOverflow->top = std::max<double>(glyphOverflow->top, -controller.minGlyphBoundingBoxY() - (glyphOverflow->computeBounds ? 0 : metricsOfPrimaryFont().ascent()));
        glyphOverflow->bottom = std::max<double>(glyphOverflow->bottom, controller.maxGlyphBoundingBoxY() - (glyphOverflow->computeBounds ? 0 : metricsOfPrimaryFont().descent()));
        glyphOverflow->left = std::max<double>(0, -controller.minGlyphBoundingBoxX());
        glyphOverflow->right = std::max<double>(0, controller.maxGlyphBoundingBoxX() - controller.totalAdvance().width());
    }
    return controller.totalAdvance().width();
}

float FontCascade::widthForCharacterInRun(const TextRun& run, unsigned characterPosition) const
{
    auto shortenedRun = run.subRun(characterPosition, 1);
    auto codePathToUse = codePath(run);
    if (codePathToUse == CodePath::Complex)
        return widthForComplexText(shortenedRun);

    return widthForSimpleText(shortenedRun);
}

void FontCascade::adjustSelectionRectForSimpleText(const TextRun& run, LayoutRect& selectionRect, unsigned from, unsigned to) const
{
    GlyphBuffer glyphBuffer;
    WidthIterator it(*this, run);
    it.advance(from, glyphBuffer);
    float beforeWidth = it.runWidthSoFar();
    it.advance(to, glyphBuffer);
    float afterWidth = it.runWidthSoFar();

    if (run.rtl()) {
        it.advance(run.length(), glyphBuffer);
        it.finalize(glyphBuffer);
        float totalWidth = it.runWidthSoFar();
        selectionRect.move(totalWidth - afterWidth, 0);
    } else {
        it.finalize(glyphBuffer);
        selectionRect.move(beforeWidth, 0);
    }
    selectionRect.setWidth(LayoutUnit::fromFloatCeil(afterWidth - beforeWidth));
}

void FontCascade::adjustSelectionRectForComplexText(const TextRun& run, LayoutRect& selectionRect, unsigned from, unsigned to) const
{
    ComplexTextController controller(*this, run);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to);
    float afterWidth = controller.runWidthSoFar();

    if (run.rtl())
        selectionRect.move(controller.totalAdvance().width() - afterWidth, 0);
    else
        selectionRect.move(beforeWidth, 0);
    selectionRect.setWidth(LayoutUnit::fromFloatCeil(afterWidth - beforeWidth));
}

void FontCascade::adjustSelectionRectForSimpleTextWithFixedPitch(const TextRun& run, LayoutRect& selectionRect, unsigned from, unsigned to) const
{
    bool whitespaceIsCollapsed = !run.allowTabs();
    float beforeWidth = widthForSimpleTextWithFixedPitch(run.text().left(from), whitespaceIsCollapsed);
    float afterWidth = widthForSimpleTextWithFixedPitch(run.text().left(to), whitespaceIsCollapsed);
    if (run.rtl()) {
        float totalWidth = widthForSimpleTextWithFixedPitch(run.text(), whitespaceIsCollapsed);
        selectionRect.move(totalWidth - afterWidth, 0);
    } else
        selectionRect.move(beforeWidth, 0);
    selectionRect.setWidth(LayoutUnit::fromFloatCeil(afterWidth - beforeWidth));
}

int FontCascade::offsetForPositionForSimpleText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    float delta = x;

    WidthIterator it(*this, run);
    GlyphBuffer localGlyphBuffer;
    unsigned offset;
    if (run.rtl()) {
        delta -= widthForSimpleText(run);
        while (1) {
            offset = it.currentCharacterIndex();
            float w;
            if (!it.advanceOneCharacter(w, localGlyphBuffer))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = it.currentCharacterIndex();
            float w;
            if (!it.advanceOneCharacter(w, localGlyphBuffer))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    it.finalize(localGlyphBuffer);
    return offset;
}

int FontCascade::offsetForPositionForComplexText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    ComplexTextController controller(*this, run);
    return controller.offsetForPosition(x, includePartialGlyphs);
}

#if !PLATFORM(COCOA) && !USE(HARFBUZZ)
// FIXME: Unify this with the macOS and iOS implementation.
RefPtr<const Font> FontCascade::fontForCombiningCharacterSequence(StringView stringView) const
{
    ASSERT(stringView.length() > 0);
    char32_t baseCharacter = *stringView.codePoints().begin();
    GlyphData baseCharacterGlyphData = glyphDataForCharacter(baseCharacter, false, NormalVariant);

    if (!baseCharacterGlyphData.isValid())
        return nullptr;
    return baseCharacterGlyphData.font.get();
}
#endif

struct GlyphIterationState {
    FloatPoint startingPoint;
    FloatPoint currentPoint;
    float y1;
    float y2;
    float minX;
    float maxX;
};

static std::optional<float> findIntersectionPoint(float y, FloatPoint p1, FloatPoint p2)
{
    if ((p1.y() < y && p2.y() > y) || (p1.y() > y && p2.y() < y))
        return p1.x() + (y - p1.y()) * (p2.x() - p1.x()) / (p2.y() - p1.y());
    return std::nullopt;
}

static void updateX(GlyphIterationState& state, float x)
{
    state.minX = std::min(state.minX, x);
    state.maxX = std::max(state.maxX, x);
}

// This function is called by CGPathApply and is therefore invoked for each
// contour in a glyph. This function models each contours as a straight line
// and calculates the intersections between each pseudo-contour and
// two horizontal lines (the upper and lower bounds of an underline) found in
// GlyphIterationState::y1 and GlyphIterationState::y2. It keeps track of the
// leftmost and rightmost intersection in GlyphIterationState::minX and
// GlyphIterationState::maxX.
static void findPathIntersections(GlyphIterationState& state, const PathElement& element)
{
    bool doIntersection = false;
    FloatPoint point = FloatPoint();
    switch (element.type) {
    case PathElement::Type::MoveToPoint:
        state.startingPoint = element.points[0];
        state.currentPoint = element.points[0];
        break;
    case PathElement::Type::AddLineToPoint:
        doIntersection = true;
        point = element.points[0];
        break;
    case PathElement::Type::AddQuadCurveToPoint:
        doIntersection = true;
        point = element.points[1];
        break;
    case PathElement::Type::AddCurveToPoint:
        doIntersection = true;
        point = element.points[2];
        break;
    case PathElement::Type::CloseSubpath:
        doIntersection = true;
        point = state.startingPoint;
        break;
    }
    if (!doIntersection)
        return;
    if (auto intersectionPoint = findIntersectionPoint(state.y1, state.currentPoint, point))
        updateX(state, *intersectionPoint);
    if (auto intersectionPoint = findIntersectionPoint(state.y2, state.currentPoint, point))
        updateX(state, *intersectionPoint);
    if ((state.currentPoint.y() >= state.y1 && state.currentPoint.y() <= state.y2)
        || (state.currentPoint.y() <= state.y1 && state.currentPoint.y() >= state.y2))
        updateX(state, state.currentPoint.x());
    state.currentPoint = point;
}

class GlyphToPathTranslator {
public:
    GlyphToPathTranslator(const TextRun& textRun, const GlyphBuffer& glyphBuffer, const FloatPoint& textOrigin)
        : m_textRun(textRun)
        , m_glyphBuffer(glyphBuffer)
        , m_fontData(glyphBuffer.fontAt(m_index))
        , m_translation(AffineTransform::makeTranslation(toFloatSize(textOrigin)))
    {
#if USE(CG)
        m_translation.flipY();
#endif
    }

    bool containsMorePaths() { return m_index != m_glyphBuffer.size(); }
    Path path();
    std::pair<float, float> extents();
    GlyphUnderlineType underlineType();
    void advance();

private:
    unsigned m_index { 0 };
    CheckedRef<const TextRun> m_textRun;
    const GlyphBuffer& m_glyphBuffer;
    Ref<const Font> m_fontData;
    AffineTransform m_translation;
};

Path GlyphToPathTranslator::path()
{
    Path path = Ref { m_fontData }->pathForGlyph(m_glyphBuffer.glyphAt(m_index));
    path.transform(m_translation);
    return path;
}

std::pair<float, float> GlyphToPathTranslator::extents()
{
    auto beginning = m_translation.mapPoint(FloatPoint(0, 0));
    auto advance = m_glyphBuffer.advanceAt(m_index);
    auto end = m_translation.mapSize(size(advance));
    return std::make_pair(beginning.x(), beginning.x() + end.width());
}

auto GlyphToPathTranslator::underlineType() -> GlyphUnderlineType
{
    return computeUnderlineType(m_textRun, m_glyphBuffer, m_index);
}

void GlyphToPathTranslator::advance()
{
    GlyphBufferAdvance advance = m_glyphBuffer.advanceAt(m_index);
    m_translation.translate(size(advance));
    ++m_index;
    if (m_index < m_glyphBuffer.size())
        m_fontData = m_glyphBuffer.fontAt(m_index);
}

Vector<FloatSegment> FontCascade::lineSegmentsForIntersectionsWithRect(const TextRun& run, const FloatPoint& textOrigin, const FloatRect& lineExtents) const
{
    Vector<FloatSegment> result;
    if (isLoadingCustomFonts())
        return result;

    auto glyphBuffer = layoutText(codePath(run), run, 0, run.length());
    if (!glyphBuffer.size())
        return result;

    FloatPoint origin = textOrigin + WebCore::size(glyphBuffer.initialAdvance());
    GlyphToPathTranslator translator(run, glyphBuffer, origin);
    for (; translator.containsMorePaths(); translator.advance()) {
        GlyphIterationState info = { FloatPoint(0, 0), FloatPoint(0, 0), lineExtents.y(), lineExtents.y() + lineExtents.height(), lineExtents.x() + lineExtents.width(), lineExtents.x() };
        switch (translator.underlineType()) {
        case GlyphUnderlineType::SkipDescenders: {
            Path path = translator.path();
            path.applyElements([&](const PathElement& element) {
                findPathIntersections(info, element);
            });
            if (info.minX < info.maxX)
                result.append({ info.minX - lineExtents.x(), info.maxX - lineExtents.x() });
            break;
        }
        case GlyphUnderlineType::SkipGlyph: {
            std::pair<float, float> extents = translator.extents();
            result.append({ extents.first - lineExtents.x(), extents.second - lineExtents.x() });
            break;
        }
        case GlyphUnderlineType::DrawOverGlyph:
            // Nothing to do
            break;
        }
    }
    return result;
}

bool shouldSynthesizeSmallCaps(bool dontSynthesizeSmallCaps, const Font* nextFont, char32_t baseCharacter, std::optional<char32_t> capitalizedBase, FontVariantCaps fontVariantCaps, bool engageAllSmallCapsProcessing)
{
    if (fontVariantCaps == FontVariantCaps::Normal)
        return false;

    if (dontSynthesizeSmallCaps)
        return false;
    if (!nextFont || nextFont->isSystemFontFallbackPlaceholder())
        return false;
    if (engageAllSmallCapsProcessing && isUnicodeCompatibleASCIIWhitespace(baseCharacter))
        return false;
    if (!engageAllSmallCapsProcessing && !capitalizedBase)
        return false;
    return !nextFont->variantCapsSupportedForSynthesis(fontVariantCaps);
}

// FIXME: Capitalization is language-dependent and context-dependent and should operate on grapheme clusters instead of codepoints.
std::optional<char32_t> capitalized(char32_t baseCharacter)
{
    if (U_GET_GC_MASK(baseCharacter) & U_GC_M_MASK)
        return std::nullopt;

    char32_t uppercaseCharacter = u_toupper(baseCharacter);
    ASSERT(uppercaseCharacter == baseCharacter || (U_IS_BMP(baseCharacter) == U_IS_BMP(uppercaseCharacter)));
    if (uppercaseCharacter != baseCharacter)
        return uppercaseCharacter;
    return std::nullopt;
}

TextStream& operator<<(TextStream& ts, const FontCascade& fontCascade)
{
    ts << fontCascade.fontDescription();

    if (fontCascade.fontSelector())
        ts << ", font selector "_s << fontCascade.fontSelector();

    if (fontCascade.fonts())
        ts << ", generation "_s << fontCascade.fonts()->generation();

    return ts;
}

} // namespace WebCore
