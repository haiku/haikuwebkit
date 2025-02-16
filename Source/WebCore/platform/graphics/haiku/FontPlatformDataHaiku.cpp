/*
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
#include "FontDescription.h"
#include "FontCustomPlatformData.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include "NotImplemented.h"

#include <String.h>

#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>

namespace WebCore {
font_family FontPlatformData::m_FallbackSansSerifFontFamily= "Noto Sans";
font_family FontPlatformData::m_FallbackSerifFontFamily = "Noto Serif";
font_family FontPlatformData::m_FallbackFixedFontFamily = "Noto Mono";
font_family FontPlatformData::m_FallbackStandardFontFamily = "Noto Sans";

void
FontPlatformData::findMatchingFontFamily(const AtomString& familyName, font_family& fontFamily)
{
    CString familyNameUTF8 = familyName.string().utf8();
    if (BFont().SetFamilyAndStyle(familyNameUTF8.data(), 0) == B_OK)
        strlcpy(fontFamily, familyNameUTF8.data(), B_FONT_FAMILY_LENGTH);
    else {
        // If no font family is found for the given name, we use a generic font.

        AtomString lowercaseFamilyName = familyName.convertToASCIILowercase();

        // This is a known foundry with its name in some families, not necessarily monospaced
        ASCIILiteral monotype = ASCIILiteral::fromLiteralUnsafe("monotype ");
        if (lowercaseFamilyName.startsWith(monotype))
            lowercaseFamilyName = AtomString(lowercaseFamilyName.impl(), monotype.length(), lowercaseFamilyName.length() - monotype.length());

        if (lowercaseFamilyName.contains(ASCIILiteral::fromLiteralUnsafe("mono"))
            || lowercaseFamilyName.contains(ASCIILiteral::fromLiteralUnsafe("consol")))
            strlcpy(fontFamily, m_FallbackFixedFontFamily, B_FONT_FAMILY_LENGTH);
        else if (lowercaseFamilyName.contains(ASCIILiteral::fromLiteralUnsafe("sans")))
            strlcpy(fontFamily, m_FallbackSansSerifFontFamily, B_FONT_FAMILY_LENGTH);
        else if (lowercaseFamilyName.contains(ASCIILiteral::fromLiteralUnsafe("serif")))
            strlcpy(fontFamily, m_FallbackSerifFontFamily, B_FONT_FAMILY_LENGTH);
        else {
            // This is the fallback font.
            strlcpy(fontFamily, m_FallbackStandardFontFamily, B_FONT_FAMILY_LENGTH);
        }
    }
}

static void findMatchingFontStyle(const font_family& fontFamily, bool bold, bool oblique, font_style* fontStyle)
{
    int32 numStyles = count_font_styles(const_cast<char*>(fontFamily));

    for (int i = 0; i < numStyles; i++) {
        if (get_font_style(const_cast<char*>(fontFamily), i, fontStyle) == B_OK) {
            BString styleName(*fontStyle);
            if ((oblique && bold) && (styleName == "Bold Italic" || styleName == "Bold Oblique"))
                return;
            if ((oblique && !bold) && (styleName == "Italic" || styleName == "Oblique"))
                return;
            if ((!oblique && bold) && styleName == "Bold")
                return;
            if ((!oblique && !bold) && (styleName == "Roman" || styleName == "Book"
                || styleName == "Condensed" || styleName == "Regular" || styleName == "Medium")) {
                return;
            }
        }
    }

    // Didn't find matching style
    memset(*fontStyle, 0, sizeof(font_style));
}

// #pragma mark -

FontPlatformData::FontPlatformData(const FontDescription& fontDescription, const AtomString& familyName)
{
	m_font = std::make_unique<BFont>();
    m_font->SetSize(fontDescription.computedSize());

    font_family fontFamily;
    findMatchingFontFamily(familyName, fontFamily);

    font_style fontStyle;
    findMatchingFontStyle(fontFamily, fontDescription.weight() == boldWeightValue(), fontDescription.italic() != std::nullopt, &fontStyle);

    m_font->SetFamilyAndStyle(fontFamily, fontStyle);

    m_size = m_font->Size();
}


FontPlatformData::FontPlatformData(const BFont& font, const FontDescription& fontDescription)
{
    m_font = std::make_unique<BFont>(font);
    m_font->SetSize(fontDescription.computedSize());

    font_family fontFamily;
    font_style fontStyle;

    font.GetFamilyAndStyle(&fontFamily, &fontStyle);

    findMatchingFontStyle(fontFamily, fontDescription.weight() == boldWeightValue(), fontDescription.italic() != std::nullopt, &fontStyle);

    m_font->SetFamilyAndStyle(fontFamily, fontStyle);

    m_size = m_font->Size();
}


void FontPlatformData::updateSize(float size)
{
	m_size = size;
	m_font->SetSize(size);
}


unsigned FontPlatformData::hash() const
{
	float tmp;
	unsigned result = 0;

	result ^= m_font->FamilyAndStyle();
	result ^= m_font->Spacing() << 24;
	result ^= m_font->Encoding() << 16;
	result ^= m_font->Face();

	tmp = m_font->Size();
	result ^= *(unsigned*)&tmp;

	tmp = m_font->Shear();
	result ^= *(unsigned*)&tmp;

	tmp = m_font->Rotation();
	result ^= *(unsigned*)&tmp;

	tmp = m_font->FalseBoldWidth();
	result ^= *(unsigned*)&tmp;

	return result;
}


bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
	if (m_font == nullptr && other.m_font == nullptr)
		return true;
	if (m_font == nullptr || other.m_font == nullptr)
		return false;
	return (*m_font == *other.m_font);
}

bool FontPlatformData::isFixedPitch() const
{
	if (m_font == nullptr)
		return false;
	return m_font->Spacing() == B_FIXED_SPACING;
}

void
FontPlatformData::SetFallBackSerifFont(const BString& font)
{
	strlcpy(m_FallbackSerifFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

void
FontPlatformData::SetFallBackSansSerifFont(const BString& font)
{
	strlcpy(m_FallbackSansSerifFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

void
FontPlatformData::SetFallBackFixedFont(const BString& font)
{
	strlcpy(m_FallbackFixedFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

void
FontPlatformData::SetFallBackStandardFont(const BString& font)
{
	strlcpy(m_FallbackStandardFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
	UNUSED_PARAM(table);
	notImplemented();
	return nullptr;
}

String FontPlatformData::description() const
{
    return familyName();
}

String FontPlatformData::familyName() const
{
	font_family family;
	font_style style;
	m_font->GetFamilyAndStyle(&family, &style);
	return String::fromUTF8(family);
}

std::optional<FontPlatformData> FontPlatformData::fromIPCData(float, FontOrientation&&, FontWidthVariant&&, TextRenderingMode&&, bool, bool, IPCData&&)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

FontPlatformData::IPCData FontPlatformData::toIPCData() const
{
    ASSERT_NOT_REACHED();
    return FontPlatformSerializedData { };
}

} // namespace WebCore

