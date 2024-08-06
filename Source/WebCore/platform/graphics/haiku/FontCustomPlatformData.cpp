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
#include "NotImplemented.h"
#include "SharedBuffer.h"

#include "wtf/RefPtr.h"

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
	m_font.UnloadFont();
	delete_area(m_area);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& description, bool bold, bool italic, const FontCreationContext&)
{
    return FontPlatformData(m_font, description);
}

RefPtr<FontCustomPlatformData> FontCustomPlatformData::create(SharedBuffer& buffer, const String& itemInCollection)
{
	void* sharedData;
	// TODO is it possible to create a SharedBuffer so that it is allocated in a clonable area already?
	// It would save a copy here.
	area_id area = create_area("web font", &sharedData, B_ANY_ADDRESS, buffer.size(), B_NO_LOCK, B_WRITE_AREA | B_READ_AREA | B_CLONEABLE_AREA);


	memcpy(sharedData, buffer.span().data(), buffer.span().size());
	BFont font;

	status_t result = font.LoadFont(area, buffer.size(), 0);

	if (result != B_OK) { // B_NOT_SUPPORTED returned for r1beta4 fallback path
		delete_area(area);
		return nullptr;
	}

    FontPlatformData::CreationData creationData = { buffer, itemInCollection };
	return adoptRef(new FontCustomPlatformData(area, font, WTFMove(creationData)));
}

RefPtr<FontCustomPlatformData> FontCustomPlatformData::createMemorySafe(SharedBuffer&, const String&)
{
    return nullptr;
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringASCIICase(format, ASCIILiteral::fromLiteralUnsafe("truetype"))
        || equalIgnoringASCIICase(format, ASCIILiteral::fromLiteralUnsafe("opentype"))
        || equalIgnoringASCIICase(format, ASCIILiteral::fromLiteralUnsafe("woff"))
    ;
}

bool FontCustomPlatformData::supportsTechnology(const FontTechnology&)
{
    // FIXME: define supported technologies for this platform (webkit.org/b/256310).
    notImplemented();
    return true;
}
}
