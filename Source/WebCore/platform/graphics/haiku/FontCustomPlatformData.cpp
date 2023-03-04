/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
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
#include "FontCustomPlatformData.h"

#include "FontPlatformData.h"
#include "SharedBuffer.h"

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
	m_font.UnloadFont();
	delete_area(m_area);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& description, bool& bold, bool& italic, const FontCreationContext&)
{
    return FontPlatformData(m_font, description);
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String& description)
{
	void* sharedData;
	area_id area = create_area("web font", &sharedData, B_ANY_ADDRESS, buffer.size(), B_NO_LOCK, B_WRITE_AREA | B_READ_AREA | B_CLONEABLE_AREA);


	memcpy(sharedData, buffer.data(), buffer.size());
	BFont font;

	status_t result = font.LoadFont(area, buffer.size(), 0);

	if (result != B_OK) { // B_NOT_SUPPORTED returned for r1beta4 fallback path
		delete_area(area);
		return std::unique_ptr<FontCustomPlatformData>(nullptr);
	}

	return std::make_unique<FontCustomPlatformData>(area, font);
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringASCIICase(format, ASCIILiteral::fromLiteralUnsafe("truetype"))
        || equalIgnoringASCIICase(format, ASCIILiteral::fromLiteralUnsafe("opentype"))
        || equalIgnoringASCIICase(format, ASCIILiteral::fromLiteralUnsafe("woff"))
    ;
}

}
